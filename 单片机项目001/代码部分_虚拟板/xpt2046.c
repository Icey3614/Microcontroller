#include "xpt2046.h"

// 显式声明单总线引脚，用于释放总线互斥
sbit DQ_Isolation = P3^7;

// 重新映射引脚名称，方便理解
sbit ADC_CS  = P3^5;
sbit ADC_CLK = P3^6;
sbit ADC_DIO = P3^7; // DI和DO复用同一根线

/*******************************************************************************
* 函 数 名: Read_AD_Data
* 函数功能: 针对 Proteus 普及版 ADC0832 设计的标准 SPI 8位光敏读取驱动
* 返 回 值: 8位AD转换结果 (0~255)
*******************************************************************************/
u16 Read_AD_Data(u8 cmd)
{
    u8 i;
    u8 dat_odd = 0;
    u8 dat_even = 0;
    
    // 传递的 cmd 参数在这里用作通道选择占位符，保持 main.c 兼容不报错
    (void)cmd; 

    // 1. 互斥总线安全隔离保护：释放单总线 DQ（51单片机准双向口写1即释放总线）
    DQ_Isolation = 1;

    ADC_CLK = 0;
    ADC_DIO = 1;
    ADC_CS  = 0; // 激活片选，启动芯片

    // 2. 按照 ADC0832 的工业时序发送起始与通道配置选择指令
    // 第1个时钟脉冲：起始位 (Start Bit = 1)
    ADC_CLK = 1; _nop_(); ADC_CLK = 0;

    // 第2个时钟脉冲：选择单端输入模式 (SGL = 1)
    ADC_DIO = 1;
    ADC_CLK = 1; _nop_(); ADC_CLK = 0;

    // 第3个时钟脉冲：选择通道 CH0 (ODD/SIGN = 0)
    ADC_DIO = 0;
    ADC_CLK = 1; 
    _nop_(); 
    ADC_CLK = 0;

    // 此时单片机释放 DIO 总线，准备转为输入状态接收数据
    ADC_DIO = 1; 

    // 3. 顺沿时钟读取由高位到低位 (MSB First) 的数据字节
    for (i = 0; i < 8; i++)
    {
        ADC_CLK = 1; _nop_(); ADC_CLK = 0; // 下降沿芯片输出数据
        dat_odd <<= 1;
        if (ADC_DIO) dat_odd |= 0x01;
    }

    // 4. 顺沿时钟读取由低位到高位 (LSB First) 的校对数据字节
    for (i = 0; i < 8; i++)
    {
        dat_even >>= 1;
        if (ADC_DIO) dat_even |= 0x80;
        ADC_CLK = 1; _nop_(); ADC_CLK = 0;
    }

    ADC_CS = 1;  // 释放片选，结束转换
    DQ_Isolation = 1; // 再次显式拉高，把总线控制权完璧归赵还给 DS18B20

    // 校验双向读取的数据是否一致，若通过则直接返回最终亮度数值
    if (dat_odd == dat_even)
    {
        return (u16)dat_odd; 
    }
    return 0;
}