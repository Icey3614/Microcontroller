#ifndef __UART_H__
#define __UART_H__

#include "config.h"

// 函数声明
void UART_Init(void);                    // 初始化串口（9600bps）
void UART_SendByte(u8 dat);              // 发送一个字节
void UART_SendString(u8 *str);           // 发送字符串
u8 UART_ReceiveByte(void);               // 接收一个字节（非阻塞，0xFF表示无数据）
u8  UART_Service(void);                 // 串口服务函数，返回接收数据（无数据返回0xFF）
void UART_SetMode(bit enter);            // 进入/退出串口模式

#endif
