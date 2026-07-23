#ifndef __LCD1602_H__
#define __LCD1602_H__

#include "config.h" //嵌套包含/头文件依赖

// LCD1602 引脚定义
sbit LCD1602_RS = P2^6;    // 数据/命令选择
sbit LCD1602_RW = P2^5;    // 读/写选择 (与蜂鸣器共用P2^5)
sbit LCD1602_E  = P2^7;    // 使能信号
#define LCD1602_DATA_PIN P0 // 数据端口

// 函数声明
void LCD1602_init(void);
void LCD1602_clear(void);
void LCD1602_write_cmd(u8 cmd);
void LCD1602_write_data(u8 dat);
void LCD1602_Position(u8 row, u8 col);
void LCD1602_display(u8 row, u8 col, u8 *str);
void LCD1602_display_char(u8 row, u8 col, u8 ch);
void delay_ms(u16 time);

#endif
