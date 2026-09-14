#include "xpt2046.h"

// 显式声明单总线引脚，用于释放总线 [cite: 1316, 1317]
sbit DQ_Isolation = P3^7;

static void SPI_Write(u8 dat)
{
    u8 i; // [cite: 1320]
    XPT_CLK = 0; // [cite: 1321]
    for (i = 0; i < 8; i++) // [cite: 1322]
    {
        XPT_DIN = dat >> 7; // 放置最高位 [cite: 1324]
        dat <<= 1; // [cite: 1325]
        XPT_CLK = 0; // [cite: 1326]
        XPT_CLK = 1;        // 上升沿锁存 [cite: 1327]
    }
}

static u16 SPI_Read(void)
{
    u16 i, dat = 0; // [cite: 1332]
    XPT_CLK = 0; // [cite: 1333]
    for (i = 0; i < 12; i++) // [cite: 1334]
    {
        dat <<= 1; // [cite: 1336]
        XPT_CLK = 1; // [cite: 1337]
        XPT_CLK = 0; // 下降沿读取 [cite: 1338]
        dat |= XPT_DOUT; // [cite: 1339]
    }
    return dat; // [cite: 1341]
}

u16 Read_AD_Data(u8 cmd)
{
    u8 i; // [cite: 1345]
    u16 AD_Value; // [cite: 1346]

    // 隔离保护：先将单总线DQ释放（51单片机准双向口写1即释放总线） [cite: 1347]
    DQ_Isolation = 1; // [cite: 1348]

    XPT_CLK = 0; // [cite: 1349]
    XPT_CS  = 0; // 激活片选 [cite: 1350]
    SPI_Write(cmd); // [cite: 1351]

    for (i = 6; i > 0; i--); // 小幅延时等待转换 [cite: 1352]

    XPT_CLK = 1; // 产生清除BUSY的时钟 [cite: 1353]
    _nop_(); _nop_(); // [cite: 1354]
    XPT_CLK = 0; // [cite: 1355]
    _nop_(); _nop_(); // [cite: 1356]

    AD_Value = SPI_Read(); // [cite: 1357]
    XPT_CS = 1;  // 释放片选 [cite: 1358]

    // 再次显式释放总线防止寄生漏电 [cite: 1359]
    DQ_Isolation = 1; // [cite: 1360]

    return AD_Value; // [cite: 1361]
}