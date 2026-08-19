#include "key.h"

#define KEY_PRESSED    1
#define KEY_UNPRESSED  0

#define KEY_TIME_LONG    1000  // 刚好对应你要求的1秒判定
#define KEY_TIME_REPEAT  200

u8 idata key_flag[KEY_COUNT];

u8 key_check(u8 key_n, u8 flag)
{
    if (key_flag[key_n] & flag) {
        if (flag != KEY_HOLD) key_flag[key_n] &= ~flag; 
        return 1;
    }
    return 0;
}

static u8 key_getState(u8 KEY_n)
{
    if (KEY_n == KEY_1) return (key_1_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    if (KEY_n == KEY_2) return (key_2_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    if (KEY_n == KEY_3) return (key_3_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    if (KEY_n == KEY_4) return (key_4_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    return KEY_UNPRESSED;
}

void key_tick(void)
{
    static u8 idata currState[KEY_COUNT] = {KEY_UNPRESSED};
    static u8 idata preState[KEY_COUNT] = {KEY_UNPRESSED};
    static u8 idata scan_div = 0;
    static u8 idata state[KEY_COUNT] = {0};
    static u16 idata time_dec[KEY_COUNT] = {0};
    u8 i;

    for (i = 0; i < KEY_COUNT; i++) {
        if (time_dec[i] > 0) time_dec[i]--;
    }

    scan_div++;
    if (scan_div >= 10) // 10ms物理消抖周期
    {
        scan_div = 0;
        for (i = 0; i < KEY_COUNT; i++)
        {
            preState[i] = currState[i];
            currState[i] = key_getState(i);

            if (currState[i] == KEY_PRESSED) key_flag[i] |= KEY_HOLD;
            else key_flag[i] &= ~KEY_HOLD;

            if (currState[i] == KEY_PRESSED && preState[i] == KEY_UNPRESSED) key_flag[i] |= KEY_DOWN;
            if (currState[i] == KEY_UNPRESSED && preState[i] == KEY_PRESSED) key_flag[i] |= KEY_UP;

            switch (state[i])
            {
                case 0:
                    if (currState[i] == KEY_PRESSED) { time_dec[i] = KEY_TIME_LONG; state[i] = 1; }
                    break;
                case 1: // 等待长按时间判定
                    if (currState[i] == KEY_UNPRESSED) { key_flag[i] |= KEY_SINGLE; state[i] = 0; }
                    else if (time_dec[i] == 0) { key_flag[i] |= KEY_LONG; time_dec[i] = KEY_TIME_REPEAT; state[i] = 2; }
                    break;
                case 2: // 长按连发状态
                    if (currState[i] == KEY_UNPRESSED) { state[i] = 0; }
                    else if (time_dec[i] == 0) { key_flag[i] |= KEY_REPEAT; time_dec[i] = KEY_TIME_REPEAT; }
                    break;
            }
        }
    }
}