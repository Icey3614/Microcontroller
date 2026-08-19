#include "ds18b20.h"
// 温度传感器模块，将温度数字化后输出
// 显式声明外部触控芯片片选，用于拉高隔离
sbit AD_CS_Isolation = P3^5;

u8 ds18B20_convertT(void)
{
    // 隔离保护：强制释放XPT2046对P3.7(DOUT)总线的可能占用 
    AD_CS_Isolation = 1; 
/*
	实验板面临引脚复用问题：
	DS18B20 的单总线和 XPT2046（AD转换芯片）的数据输出引脚都连在单片机的
	P3.7 上 。  这行代码的效果： 通过给 AD_CS_Isolation
	（也就是 XPT2046 的片选引脚 P3.5）送高电平 1
	强行让 XPT2046 芯片进入“休眠/高阻态”
	断开它与 P3.7 的连接 。这样就彻底扫清了干扰，
	确保接下来的信号只有单片机和 DS18B20 在独占通信。  
*/
    if (oneWire_init() != 0)
        return 0; // 不在线
		/*
		在跟传感器连接，单片机必须在总线上发一个复位脉冲，
		看看这个传感器到底有没有“接好”或者“睡醒” 。
		这行代码的效果： 调用 oneWire_init() 。
		如果返回 0，说明传感器有应答（在线） ；
		如果返回的值不等于 0（说明没有器件搭理单片机），
		函数就立刻识相地返回 0，
		告诉主控系统“传感器不在线/硬件损坏”，不要再继续往下跑了 。 
		*/
    oneWire_sendByte(SKIP_ROM);
    oneWire_sendByte(CONVERT_T);
    return 1;  // 在线
}

int ds18B20_read_temp_c100(void)
{
    u8 TLSB, TMSB;
    int temp; 

    AD_CS_Isolation = 1; // 强制隔离保护触控芯片 

    if (oneWire_init() == 0) // 有器件应答 
    {
        oneWire_sendByte(SKIP_ROM); 
        oneWire_sendByte(READ_REGISTER); 
        TLSB = oneWire_receiveByte(); 
        TMSB = oneWire_receiveByte(); 
        temp = ((int)TMSB << 8) | TLSB;

        /*
         V2.0 定点化优化（重要）：
         DS18B20 的 12 位原始数据本身就是“补码”，单位是 1/16 ℃。
         以前这里用 float：temp / 16.0f，再让主程序做 temp*100.0f 之类的浮点运算，
         会把 Keil C51 整套浮点库（约 2KB+）链接进来，Flash 被吃掉一大块。
         现在全部改用整数定点：
             温度(℃) × 100 = 原始值 × 100 / 16 = 原始值 × 25 / 4
         返回“温度×100”的整数（25.50℃ 就返回 2550），
         显示和 40℃ 报警判断全部用整数完成，体积更小、速度更快。
        */
        return (int)(((long)temp * 25L) / 4L); // 单位 0.01℃（截断取整）
    }
    return 0; // 传感器不在线，返回 0.00℃
}
