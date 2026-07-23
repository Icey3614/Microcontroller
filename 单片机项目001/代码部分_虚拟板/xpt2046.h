#ifndef __XPT2046_H__
#define __XPT2046_H__

#include "config.h"

// XPT2046 引脚定义
sbit XPT_DOUT = P3^7;    // 数据输出（与oneWire共用P3^7）
sbit XPT_CLK  = P3^6;    // 时钟
sbit XPT_DIN  = P3^4;    // 数据输入
sbit XPT_CS   = P3^5;    // 片选

// ADC通道定义
#define ADC_CH_LIGHT  0xA4   // AIN2 光敏电阻

// 函数声明
u16 Read_AD_Data(u8 cmd);

#endif
