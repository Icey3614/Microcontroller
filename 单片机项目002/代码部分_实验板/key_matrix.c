#include "key_matrix.h"

// 【修复核心】：弃用 C99 enum 枚举，改用纯 C89 标准宏定义状态，彻底解决 C141/C129 编译错误
#define KEY_IDLE      0  // 空闲态
#define KEY_DEBOUNCE  1  // 按下消抖态
#define KEY_PRESS     2  // 确认压下触发态
#define KEY_RELEASE   3  // 等待完全松手拦截态

#define KEY_DEBOUNCE_CNT 10    // 软件消抖滤波常数（10ms）[cite: 793]

static u8 idata key_scan_cnt = 0; // [cite: 794]
static u8 idata key_state = KEY_IDLE; // [cite: 795]
static u8 idata key_value = 0;       // 对外输出的有效键值 [cite: 796]
static u8 idata key_temp = 0;        // 硬件扫描中间暂存值 [cite: 797]

/*******************************************************************************
* 函 数 名: key_scan_hardware
* 功能描述: 硬件层快速线反转扫描 4x4 矩阵键盘
* 返 回 值: 按键交叉物理组合码（0x00=无按键压下）
* 引脚映射: 行 P1.7~P1.4，列 P1.3~P1.0 [cite: 802]
*******************************************************************************/
static u8 key_scan_hardware(void)
{
    u8 row, col; // [cite: 806]

    // 第一步：行引脚输出高电平，列引脚输出低电平，读取行状态
    P1 = 0xF0; // [cite: 808]
    col = P1 & 0xF0; // [cite: 809]
    if (col == 0xF0) return 0x00; // 没有任何键压下，迅速安全返回 [cite: 810]

    // 第二步：线反转，列引脚输出高电平，行引脚输出低电平，读取列状态
    P1 = 0x0F; // [cite: 812]
    row = P1 & 0x0F; // [cite: 813]

    return (col | row); // 组合成独一无二的十六进制位置组合特征码 [cite: 814]
}

/*******************************************************************************
* 函 数 名: MatrixKey_Tick
* 功能描述: 矩阵按键高灵敏度按下触发状态机（每1ms由定时器T0中断安全分发）
*******************************************************************************/
void MatrixKey_Tick(void)
{
    u8 key_code; // [cite: 822]

    switch (key_state) // [cite: 823]
    {
        case KEY_IDLE: // [cite: 825]
            key_code = key_scan_hardware(); // [cite: 826]
            if (key_code != 0x00) // [cite: 827]
            {
                key_temp = key_code;    // 临时锁定特征值 [cite: 829]
                key_scan_cnt = 0;       // 清空消抖计数器 [cite: 830]
                key_state = KEY_DEBOUNCE; // 切入消抖过滤判定 [cite: 831]
            }
            break;

        case KEY_DEBOUNCE: // [cite: 834]
            key_scan_cnt++; // [cite: 835]
            if (key_scan_cnt >= KEY_DEBOUNCE_CNT) // [cite: 836]
            {
                key_code = key_scan_hardware(); // [cite: 838]
                if (key_code == key_temp) // [cite: 839]
                    key_state = KEY_PRESS;   // 确认是真实击键，立刻切入压下触发状态 [cite: 840]
                else
                    key_state = KEY_IDLE;    // 属于噪声抖动，打回原形 [cite: 842]
            }
            break;

        case KEY_PRESS: // [cite: 845]
            // 按下瞬间立即译码生成键值，无需等待松手
            switch (key_temp) // [cite: 849]
            {
                case 0x77: key_value = MATRIX_S1;  break; // [cite: 851]
                case 0x7B: key_value = MATRIX_S2;  break; // [cite: 852]
                case 0x7D: key_value = MATRIX_S3;  break; // [cite: 853]
                case 0x7E: key_value = MATRIX_S4;  break; // [cite: 854]

                case 0xB7: key_value = MATRIX_S5;  break; // [cite: 855]
                case 0xBB: key_value = MATRIX_S6;  break; // [cite: 856]
                case 0xBD: key_value = MATRIX_S7;  break; // [cite: 857]
                case 0xBE: key_value = MATRIX_S8;  break; // [cite: 858]

                case 0xD7: key_value = MATRIX_S9;  break; // [cite: 859]
                case 0xDB: key_value = MATRIX_S10; break; // [cite: 860]
                case 0xDD: key_value = MATRIX_S11; break; // [cite: 861]
                case 0xDE: key_value = MATRIX_S12; break; // [cite: 862]

                case 0xE7: key_value = MATRIX_S13; break; // [cite: 863]
                case 0xEB: key_value = MATRIX_S14; break; // [cite: 864]
                case 0xED: key_value = MATRIX_S15; break; // [cite: 865]
                case 0xEE: key_value = MATRIX_S16; break; // [cite: 866]
                default: break; // [cite: 867]
            }
            key_state = KEY_RELEASE; // 键值生成完毕，无缝跳转至松手拦截态阻止连续重发 [cite: 869]
            break;

        case KEY_RELEASE: // [cite: 872]
            key_code = key_scan_hardware(); // [cite: 846]
            if (key_code == 0x00)    // 只有当用户物理上完全松开手指，总线重归空闲时
            {
                key_state = KEY_IDLE; // 才允许退出拦截状态，准备迎接下一次击键 [cite: 873]
            }
            break;
    }
}

/*******************************************************************************
* 函 数 名: key_get_value
* 功能描述: 读取当前生成的有效矩阵按键值（读取后自动销毁）
*******************************************************************************/
u8 key_get_value(void)
{
    u8 temp = key_value; // [cite: 884]
    if (temp != 0) // [cite: 885]
        key_value = 0;       // 经典的读清零机制 [cite: 886]
    return temp; // [cite: 887]
}