#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "config.h"

// 蜂鸣器引脚定义 (P2^5, 与LCD RW共用)
sbit Beep = P2^5;

// 函数声明
void Buzzer_PlayQingHuaCi(void);     // 开始播放青花瓷（全非阻塞）
void Buzzer_Stop(void);              // 停止播放
void Buzzer_Service(void);           // 音乐状态机轮询（主循环调用）
bit  Buzzer_IsPlaying(void);          // 查询是否正在播放
void Buzzer_BeatTick(void);          // 节拍递减（由Timer0的1ms中断调用）

#endif