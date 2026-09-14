#ifndef __TIMER_H__
#define __TIMER_H__

#include "config.h"

// 定时器初始化函数
void T0_init(void);     // T0: 1ms 按键扫描
void T1_init(void);     // T1: 蜂鸣器音调 / 串口波特率
void T2_init(void);     // T2: 1ms 系统滴答

// 系统滴答相关
u16 get_current_sysTick(void);
bit sysTick_checked(u16 *last_tick, u16 interval_ms);

// 运行计时相关 (时分秒)
extern u16 idata runtime_hours;
extern u16 idata runtime_minutes;
extern u16 idata runtime_seconds;
extern u32 idata runtime_total_seconds;
void reset_runtime(void);

#endif
