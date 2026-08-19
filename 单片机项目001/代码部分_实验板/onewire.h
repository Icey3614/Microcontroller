#ifndef __ONEWIRE_H__
#define __ONEWIRE_H__

#include "config.h"

// 单总线引脚定义
sbit oneWire_DQ = P3^7;

// 函数声明
bit oneWire_init(void);
void oneWire_sendByte(u8 dat);
u8 oneWire_receiveByte(void);

#endif
