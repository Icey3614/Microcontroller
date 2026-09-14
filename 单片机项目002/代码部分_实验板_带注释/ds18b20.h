#ifndef __DS18B20_H__
#define __DS18B20_H__

#include "config.h"
#include "onewire.h"

// ROM指令
#define SKIP_ROM        0xCC
#define CONVERT_T       0x44
#define READ_REGISTER   0xBE

// 函数声明
u8 ds18B20_convertT(void);              // 启动温度转换, 返回0=不在线, 1=在线
float ds18B20_read_temperture(void);    // 读取温度值

#endif
