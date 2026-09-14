#include "key_matrix.h"
// 定义按键状态
#define KEY_IDLE      0  // 空闲态
#define KEY_DEBOUNCE  1  // 按下消抖态
#define KEY_PRESS     2  // 确认压下触发态
#define KEY_RELEASE   3  // 等待完全松手拦截态

#define KEY_DEBOUNCE_CNT 10    // 软件消抖滤波常数（10ms
/*在标准 51 单片机里，内部 RAM 只有 256 字节。
idata 专门修饰前 256 字节的直接/间接寻址区。
将这几个高频使用的状态机变量放在 idata 里，
单片机在执行流水扫描时，寻址速度最快，能做到真正的“零延迟”响应。*/
//全局静态变量
static u8 idata key_scan_cnt = 0;
static u8 idata key_state = KEY_IDLE;
static u8 idata key_value = 0;       // 对外输出的有效键值
static u8 idata key_temp = 0;        // 硬件扫描中间暂存值 

/*******************************************************************************
* 函 数 名: key_scan_hardware
* 功能描述: 硬件层快速线反转扫描 4x4 矩阵键盘
* 返 回 值: 按键交叉物理组合码（0x00=无按键压下）
* 引脚映射: 行 P1.7~P1.4，列 P1.3~P1.0 [cite: 802]
*******************************************************************************/
static u8 key_scan_hardware(void)  //确定被按下按键的行与列
{
    u8 row, col;

    // 第一步：行引脚输出高电平，列引脚输出低电平，读取行状态
    P1 = 0xF0; 
    col = P1 & 0xF0;
    if (col == 0xF0) return 0x00; // 没有任何键压下，迅速安全返回

    // 第二步：线反转，列引脚输出高电平，行引脚输出低电平，读取列状态
    P1 = 0x0F; 
    row = P1 & 0x0F; 

    return (col | row); // 按位进行或运算，有1返回1，全0返回0
}

/*******************************************************************************
* 函 数 名: MatrixKey_Tick
* 功能描述: 矩阵按键高灵敏度按下触发状态机（每1ms由定时器T0中断安全分发）
* 两层switch的嵌套，先确定矩阵按键大致的状态，在通过第二层嵌套确定键值
* 最初运行这个函数时，按键（key_state）处于默认状态，
* 在进入第一层switch中时，如果经过对比后发现与默认状态不符，
* 则说明按键状态可能发生了改变，这时将key_state转为可以进入第二种case的状态，
* 在第二个case中经过一段时间的消抖测试，发现按键的状态确实不是默认了，确实是改变了，
* 则将key_state转为可以进入第三种case的状态，之后进入第三种case之后，开
* 始具体读取是按下了哪个键，同时将key_state转为可以进入第四种case的状态，
* 第四阶段（松手拦截）：进入第四种 case 之后，单片机会拦截 。
* 只要手指还没抬起来，就一直卡在这个 case 里，
* 不执行任何业务，彻底屏蔽长按连击 。
* 直到下发硬件扫描重新变回默认的 0x00（即手指物理上完全松开）的瞬间，
* 它才会将 key_state 重新赋值为 KEY_IDLE（空闲态），
* 让整个状态机安全返回到最初的默认状态，等待下一次击键 。  
*******************************************************************************/
void MatrixKey_Tick(void)
{
    u8 key_code;

    switch (key_state)
    {
        case KEY_IDLE:
            key_code = key_scan_hardware();
				/*
				绝大多数时候，用户没有去按按键，单片机就会每 1ms 进来执行一次 key_scan_hardware()
				只要返回值是 0x00（无按键），它就什么都不做，直接退出 。
				跳转条件：一旦某 1ms 进来看见返回值不是 0x00，
				说明有人碰了键盘 ，它立刻把状态切换到下一种 
				*/
				if (key_code != 0x00) // 如果发现突然变化了状态
            {
                key_temp = key_code;    // 临时锁定特征值
                key_scan_cnt = 0;       // 清空消抖计数器
                key_state = KEY_DEBOUNCE; // 切入消抖过滤判定
            }
            break;

        case KEY_DEBOUNCE: 
            key_scan_cnt++; 
				/*
				消抖态确认，如果消抖过后来的状态还没有改变
				则确定是发生了按下
				转入按键确认case
				*/
            if (key_scan_cnt >= KEY_DEBOUNCE_CNT) 
            {
                key_code = key_scan_hardware(); 
                if (key_code == key_temp) 
                    key_state = KEY_PRESS;   // 确认是真实击键，立刻切入压下触发状态
                else
                    key_state = KEY_IDLE;    // 属于噪声抖动，打回原形 
            }
            break;

        case KEY_PRESS: 
            // 按下瞬间立即译码生成键值，无需等待松手
            switch (key_temp) 
            {
                case 0x77: key_value = MATRIX_S1;  break;
                case 0x7B: key_value = MATRIX_S2;  break; 
                case 0x7D: key_value = MATRIX_S3;  break; 
                case 0x7E: key_value = MATRIX_S4;  break; 

                case 0xB7: key_value = MATRIX_S5;  break; 
                case 0xBB: key_value = MATRIX_S6;  break; 
                case 0xBD: key_value = MATRIX_S7;  break; 
                case 0xBE: key_value = MATRIX_S8;  break; 

                case 0xD7: key_value = MATRIX_S9;  break; 
                case 0xDB: key_value = MATRIX_S10; break; 
                case 0xDD: key_value = MATRIX_S11; break;
                case 0xDE: key_value = MATRIX_S12; break; 

                case 0xE7: key_value = MATRIX_S13; break;
                case 0xEB: key_value = MATRIX_S14; break; 
                case 0xED: key_value = MATRIX_S15; break; 
                case 0xEE: key_value = MATRIX_S16; break; 
                default: break; 
            }
            key_state = KEY_RELEASE; // 键值生成完毕，无缝跳转至松手拦截态阻止连续重发 
            break;

        case KEY_RELEASE: // [cite: 872]
					//(等待完全松手拦截态) —— 防止连击刷屏
            key_code = key_scan_hardware(); // 
            if (key_code == 0x00)    // 只有当用户物理上完全松开手指，总线重归空闲时
            {
                key_state = KEY_IDLE; // 才允许退出拦截状态，准备迎接下一次击键
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
    u8 temp = key_value;
    if (temp != 0) // 
        key_value = 0;       // 经典的读清零机制
    return temp; //
}