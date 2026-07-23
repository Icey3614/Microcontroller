#include "xpt2046.h"

// 显式声明单总线引脚，用于释放总线 
sbit DQ_Isolation = P3^7;
/*
对于温度与亮度模块，
在进行使用时，会先将对方的对于总线的识别定义为被占用，
这样就可以确保自身暂时独占总线，同时避免干扰
*/

static void SPI_Write(u8 dat)
{
	/*
	这个函数的作用是：
	在专门的同步时钟信号（XPT_CLK）引导下 ，
	利用移位算法将一个 8 位字节数据按“最高位先发”的顺序 ，
	逐位输出到数据线（XPT_DIN）上并锁存进 XPT2046 芯片中 ，
	从而实现单片机向亮度芯片发送控制命令的底层操作 。
	*/
    u8 i;
    XPT_CLK = 0;
    for (i = 0; i < 8; i++)
    {
        XPT_DIN = dat >> 7; // 放置最高位
        dat <<= 1;
        XPT_CLK = 0;
        XPT_CLK = 1;        // 上升沿锁存
    }
}


static u16 SPI_Read(void)
{
    u16 i, dat = 0;
    XPT_CLK = 0;
    for (i = 0; i < 12; i++)
    {
			/*
			由于 XPT2046 是一款 12 位高精度的模数转换芯片（ADC），
			它翻译出来的数字范围是二进制的 0000 0000 0000 到 1111 1111 1111
			（对应十进制的 0 ~ 4095）。
			所以，单片机必须连续敲击 12 次时钟，才能把这 12 个位全部收进来。
			*/
        dat <<= 1;
        XPT_CLK = 1;
        XPT_CLK = 0; // 下降沿读取 
        dat |= XPT_DOUT; 
    }
    return dat; //
}

u16 Read_AD_Data(u8 cmd)
{
	//调用前面两个函数获取亮度数据，将其返回，同时释放总线
    u8 i; 
    u16 AD_Value; 

    // 隔离保护：先将单总线DQ释放（51单片机准双向口写1即释放总线）
    DQ_Isolation = 1;

    XPT_CLK = 0; 
    XPT_CS  = 0; 
    SPI_Write(cmd);

    for (i = 6; i > 0; i--); // 小幅延时等待转换

    XPT_CLK = 1; // 产生清除BUSY的时钟
    _nop_(); _nop_();
    XPT_CLK = 0; 
    _nop_(); _nop_();

    AD_Value = SPI_Read(); 
    XPT_CS = 1;  // 释放片选 

    // 再次显式释放总线防止寄生漏电
    DQ_Isolation = 1;

    return AD_Value;
}