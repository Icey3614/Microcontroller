#include "timer.h"
#include "key.h"
#include "key_matrix.h"
#include "buzzer.h"

u16 idata sysTick = 0;
u16 idata runtime_hours = 0;
u16 idata runtime_minutes = 0;
u16 idata runtime_seconds = 0;
u32 idata runtime_total_seconds = 0;
static u16 idata runtime_tick = 0;

void T0_init(void)
{
    TMOD = (TMOD & 0xF0) | 0x01; // T0 16位定时模式
    TH0 = 0xFC; TL0 = 0x66;      // 1ms 定时基准初值
    ET0 = 1; TR0 = 1;
}

void T1_init(void)
{
    TMOD = (TMOD & 0x0F) | 0x10; // T1 16位定时模式
    TH1 = 0xFC; TL1 = 0x66;
    ET1 = 0; TR1 = 0;
}

void T2_init(void) { ET2 = 0; TR2 = 0; } // 弃用并关锁 T2，防止总线死锁

void T0_ISR(void) interrupt 1
{
    TH0 = 0xFC; TL0 = 0x66; // 硬件级重装载

    MatrixKey_Tick(); // 1ms 矩阵扫描
    key_tick();       // 1ms 独立键扫描
    Buzzer_BeatTick(); // 1ms 音乐节拍倒计时

    sysTick++;
    runtime_tick++;
    if (runtime_tick >= 1000)
    {
        runtime_tick = 0;
        runtime_total_seconds++;
        runtime_seconds++;
        if (runtime_seconds >= 60) {
            runtime_seconds = 0; runtime_minutes++;
            if (runtime_minutes >= 60) { runtime_minutes = 0; runtime_hours++; }
        }
    }
}

u16 get_current_sysTick(void)
{
    u16 t;
    EA = 0; t = sysTick; EA = 1;
    return t;
}

bit sysTick_checked(u16 *last_tick, u16 interval_ms)
{
    u16 current = get_current_sysTick();
    if (*last_tick == 0) { *last_tick = current; return 0; }
    if (current - *last_tick >= interval_ms) { *last_tick = current; return 1; }
    return 0;
}

void reset_runtime(void)
{
    runtime_hours = 0; runtime_minutes = 0; runtime_seconds = 0;
    runtime_total_seconds = 0; runtime_tick = 0;
}