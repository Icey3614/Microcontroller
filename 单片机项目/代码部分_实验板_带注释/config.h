#ifndef __CONFIG_H__ //如果没有定义...
#define __CONFIG_H__ //那么立刻定义...
//基本配置设置
#include <reg52.h>
#include <intrins.h>  //内联函数

// 基本类型定义
typedef unsigned long u32;  //定义长整型
typedef unsigned int u16;   //定义短整型
typedef unsigned char u8;   //定义字符型

// 系统晶振频率
#define SYSTEM_OSC 11059200  // 定义晶振频率@11.0592MHz

#endif //结束判断
