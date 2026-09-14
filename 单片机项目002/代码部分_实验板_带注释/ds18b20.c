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

float ds18B20_read_temperture(void)
{
    u8 TLSB, TMSB;
    int temp; 
    float temperture = 0.0f; 

    AD_CS_Isolation = 1; // 强制隔离保护触控芯片 

    if (oneWire_init() == 0) // 有器件应答 
    {
        oneWire_sendByte(SKIP_ROM); 
        oneWire_sendByte(READ_REGISTER); 
        TLSB = oneWire_receiveByte(); 
        TMSB = oneWire_receiveByte(); 
        temp = ((int)TMSB << 8) | TLSB;

        // 处理正负温度 
        if (TMSB & 0xF8) 
        {
            temp = ~temp + 1; 
            temperture = -temp / 16.0f;
        }
        else // [cite: 1172]
        {
            temperture = temp / 16.0f;
        }
    }
    return temperture; 
}