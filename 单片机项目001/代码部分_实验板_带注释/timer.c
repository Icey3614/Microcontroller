#include "timer.h"
#include "key.h"
#include "key_matrix.h"
#include "buzzer.h"

u16 idata sysTick = 0;  //无static，即全局可以访问
u16 idata runtime_hours = 0;//时
u16 idata runtime_minutes = 0;//分
u16 idata runtime_seconds = 0;//秒
u32 idata runtime_total_seconds = 0;//总运行秒
static u16 idata runtime_tick = 0;  //只有在timer.c中可以访问
/*
它的任务，就是在单片机刚开机的时候，
把定时器0配置成你需要的“1毫秒节拍器”模式，
为整个系统的按键扫描和事件倒计时提供精准的时间基准 。
定时器：
每次定时器溢出之后会触发中断，这时会向CPU发出中断请求，CPU就会转向中断程序，
这时将一些想要每个一段时间的执行其中的程序放入其中，
就可以实现每隔一段时间就执行的效果
也就是说定时器的计数不是像delay一样在CPU中执行的，
而是有一个专门的寄存器。只要寄存器溢出，发出中断请求即可
当然不是随便编写一个代码就可使用定时器还需要用到
    ET0 = 1;
		TR0 = 1;
这样的句式写在代码中，这样才会启动定时器
*/
void T0_init(void)
{
    TMOD = (TMOD & 0xF0) | 0x01; // T0 16位定时模式
		// 8位特殊功能寄存器
    TH0 = 0xFC;      // (Timer 0 High Byte)定时器0的高8位。
		TL0 = 0x66;      // (Timer 0 Low Byte)定时器0的低8位。1ms 定时基准初值
	/*
	给定时器的计数寄存器赋初值
	因为 16 位寄存器放不下，所以拆开来放：
	高8位 TH0 放 0xFC，低8位 TL0 放 0x66 。  
	*/
    ET0 = 1; //打开中断允许开关
		TR0 = 1; //摁下启动秒表
}

/*
让蜂鸣器按要求运行：
定时器1 在项目里主要负责给蜂鸣器（Buzzer）提供高频的翻转电平来发声。
如果开机直接 TR1 = 1; ET1 = 1;，蜂鸣器会在开机瞬间发出刺耳的盲音。
*/
void T1_init(void)
{
    TMOD = (TMOD & 0x0F) | 0x10; // T1 16位定时模式
    TH1 = 0xFC; TL1 = 0x66;
    ET1 = 0; TR1 = 0;
}

void T2_init(void) { ET2 = 0; TR2 = 0; } // 弃用并关锁 T2，防止总线死锁
// 在整个项目中不使用定时器2

/*
定时器0的中断程序：
定时器0在开机后，每1ms就自动执行一次，
同时这个中断程序也是每1ms执行一次
*/
void T0_ISR(void) interrupt 1
{
    TH0 = 0xFC; TL0 = 0x66; // 硬件级重装载

    MatrixKey_Tick(); // 1ms 矩阵扫描
    key_tick();       // 1ms 独立键扫描
    Buzzer_BeatTick(); // 1ms 音乐节拍倒计时

    sysTick++;  //1ms滴答
    runtime_tick++; //运行时间滴答（局部）
		//时分秒的计算
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

//无中断影响的获取当前的滴答时间戳
u16 get_current_sysTick(void)
{
    u16 t;
    EA = 0; t = sysTick; EA = 1;//关；记录；开
    return t;
}

//u16，无符号型数
bit sysTick_checked(u16 *last_tick, u16 interval_ms)
{
    u16 current = get_current_sysTick();//获得当前的时间戳
    if (*last_tick == 0) { *last_tick = current; return 0; }
		//含义：如果传入的闹钟变量 *last_tick 还是 0（说明是刚开机第一次运行这个功能）
    //它会先把当前的系统时间 current 赋值给这个闹钟变量存起来 ， 
		//然后返回 0（代表时间还没到） 。
    if (current - *last_tick >= interval_ms) { *last_tick = current; return 1; }
		//时间间隔大于规定计数，开始执行后续程序
    return 0;
		//时间间隔还没到，继续计时
}

//时间重置，关机时调用
void reset_runtime(void)
{
    runtime_hours = 0; runtime_minutes = 0; runtime_seconds = 0;
    runtime_total_seconds = 0; runtime_tick = 0;
}