#ifndef __KEY_H__
#define __KEY_H__

#include "config.h"

// 独立按键定义
#define KEY_COUNT    4

#define KEY_1        0    // K1 - P30 开关机
#define KEY_2        1    // K2 - P31 回主界面
#define KEY_3        2    // K3 - P32 Hello World
#define KEY_4        3    // K4 - P33 保留

// 按键动作标志位
#define KEY_HOLD     0x01   // 按住
#define KEY_DOWN     0x02   // 按下
#define KEY_UP       0x04   // 松开
#define KEY_SINGLE   0x08   // 单击
#define KEY_DOUBLE   0x10   // 双击
#define KEY_LONG     0x20   // 长按
#define KEY_REPEAT   0x40   // 连击

// 独立按键引脚定义
sbit key_1_pin = P3^0;   // K1 P30
sbit key_2_pin = P3^1;   // K2 P31
sbit key_3_pin = P3^2;   // K3 P32
sbit key_4_pin = P3^3;   // K4 P33

// 函数声明
void key_tick(void);
u8 key_check(u8 key_n, u8 flag);

#endif
