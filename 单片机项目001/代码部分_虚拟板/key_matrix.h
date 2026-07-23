#ifndef __KEY_MATRIX_H__
#define __KEY_MATRIX_H__

#include "config.h"

// 矩阵按键值定义
// S1 S2 S3 S4
// S5 S6 S7 S8
// S9 S10 S11 S12
// S13 S14 S15 S16
#define MATRIX_S1   1
#define MATRIX_S2   2
#define MATRIX_S3   3
#define MATRIX_S4   4
#define MATRIX_S5   5
#define MATRIX_S6   6
#define MATRIX_S7   7
#define MATRIX_S8   8
#define MATRIX_S9   9
#define MATRIX_S10  10
#define MATRIX_S11  11
#define MATRIX_S12  12
#define MATRIX_S13  13
#define MATRIX_S14  14
#define MATRIX_S15  15
#define MATRIX_S16  16

// 函数声明
void MatrixKey_Tick(void);              // 每1ms调用
u8 key_get_value(void);                // 获取键值，读取后清零

#endif
