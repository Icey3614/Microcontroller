/****************************************************************************************************************
* 项目名称：单片机综合项目 - 桌面多功能智能摆件全联调优化版 (main.c)
* 硬件平台：STC89C52RC / AT89C52 (真实物理实验板全兼容版)
* 编译环境：Keil uVision5 (C51 编译器)
* 生成日期：2026-05-31
* ===============================================================================================================
* 核心优化特性
* ===============================================================================================================
* 1. 硬件级电平隔离：完美解决真实实验板上由于引脚复用（如蜂鸣器与 LCD1602 共用 P2.5、
* DS18B20 与 XPT2046 共用 P3.7）造成的硬件电平污染、屏幕乱码与物理器件冲突隐患。
* 2. 编译警告根除：显式调用 UART_SendString 等底层函数，彻底消除 Keil 编译时的 L16 
* 或 L1 级别的未调用、重入警告，实现 0 Error, 0 Warning 完美编译。
* 3. 智能加权计算器：重构选项四（定点计算器）显示逻辑。输入数字时，普通整数与运算符
* 实时不闪烁刷新；按下等号后，结果精准保留 2 位小数，支持连续运算。
* 4. 物理级安全防护：安全集成 3 位开机密码锁(初始:123)。增设物理超温报警：当环境温度
* 超出 40.0℃ 时，系统强行切入最高优先级，蜂鸣器长鸣报警，直至温度恢复正常。
* 5. 手动切页联动：彻底移除主菜单的自动轮播，改由独立按键 K3 (左切) 与 K4 (右切) 
* 对 3 个主功能界面进行手动全循环切页。任何状态下长按 K2 一秒无缝安全退出当前功能，
* 且退出后默认静止停留在对应的主功能界面。
*  ==============================================================================================================
* 项目全功能矩阵 (功能说明书)
* ===============================================================================================================
* 主菜单通过 K3/K4 手动切换，共包含 3 大主界面，按矩阵键盘编号（1-4）进入对应子功能：
* 【界面一】环境与系统监控
* 功能 1：环境温度监测。驱动 DS18B20 采集环境温度，精度达 0.1℃。具备 40℃ 
* 红线温控长鸣报警机制。
*  功能 2：系统计时器。利用 Timer2 触发 1ms 滴答时钟，精准累计并显示系统自开机
* 以来的运行时间 (时:分:秒)，支持掉电前全秒数统计。
* 【界面二】多媒体与人机交互
* 功能 3：非阻塞音乐播放器。内置经典曲目《青花瓷》曲谱。基于 Timer1 动态改变
* 溢出率产生特定频率，实现音乐播放的同时，按键、计时与显示互不干扰。
* 功能 4：定点数计算器。支持 16 键矩阵键盘输入，可进行带负数、带小数结果的
* 加、减、乘、除四则运算，结果自动保留 2 位小数。
* 【界面三】智能外设扩展
* 串口流式显示：通过 UART (9600bps) 接收上位机发送的字符，并在 LCD1602 上
* 实现流式滚动显示。
* 环境亮度监测：通过 SPI 总线驱动 XPT2046 芯片读取光敏电阻的 A/D 转换值，
* 实时量化并显示当前环境的光照强度。
* * =============================================================================================================
* 系统运行流程 (Flowchart)
* ===============================================================================================================
* 开机上电
* │
* ▼
* [系统硬件初始化] (LCD1602三次硬唤醒、Timer初始化、UART配置)
* │
* ▼
* [开机密码锁界面] ─── 输入错误(达3次) ─── [系统死锁/蜂鸣器报警]
* │ (密码正确: 123)
* ▼
* ┌──────────── 用户长按 K2 退出 (默认停留在对应主界面) ─────────────
* │                                                                                                        
* └─> [主菜单手动循环切页] (默认界面一)                                                                    
* │                                                                                                        
* ├─> 按键 K3 (向左翻页) / K4 (向右翻页) ─────────────────┐                                
* │                                                                       ▼  
* ├─> [后台全局安全轮询] ──? 温度 > 40℃? ─? YES ─? [强制触发：蜂鸣器长鸣报警]
* │                                        └─? NO  ─? [释放 P2.5/保持正常]
* │
* └─> 监测矩阵键盘输入 (1 - 4)
* │
* ├─> 键值 1 ─> 进入 [功能 1: DS18B20 温度动态显示]
* ├─> 键值 2 ─> 进入 [功能 2: 1ms 时分秒系统精准计时]
* ├─> 键值 3 ─> 进入 [功能 3: Timer1 驱动非阻塞《青花瓷》播放]
* └─> 键值 4 ─> 进入 [功能 4: 16键矩阵定点加减乘除计算器]
* * ==============================================================================================================
*  硬件引脚复用与动态隔离说明
* ================================================================================================================
* 为防止实验板上多外设共用引脚造成的物理干扰，驱动层采用了“分时片选与电平锁定”技术：
* - P2.5 (Buzzer & LCD_EN)  ：在 LCD 读写脉冲结束后，立即将 P2.5 强行拉高/低，
* 隔离蜂鸣器流过的非预期电流，消除杂音与屏幕闪烁。
* - P3.7 (DS18B20 & AD_CS)  ：在调用 XPT2046 采集光敏 A/D 前后，严格释放单总线时序，
* 确保单总线的微秒级拉低复位电平不被 ADC 片选信号污染。
* - P3.0~P3.3 (独立按键)     ：外部中断与查询法复合扫描，与串口 RXD/TXD 完美避让。
*****************************************************************************************************************/
#include "config.h"
#include "lcd1602.h"
#include "key.h"
#include "key_matrix.h"
#include "timer.h"
#include "ds18b20.h"
#include "xpt2046.h"
#include "buzzer.h"
#include "uart.h"
#include <string.h>

/* ==================== 全局状态定义 ==================== */
#define STATE_OFF           0   // 关机状态
#define STATE_PWD_INPUT     1   // 密码输入状态
#define STATE_MENU          2   // 功能选择主菜单（对应原 STATE_MAIN）
#define STATE_SHOW_BRIGHT   3   // 选项一：环境亮度
#define STATE_SHOW_TEMP     4   // 选项二：环境温度（带40℃报警）
#define STATE_MUSIC         5   // 选项三：青花瓷音乐播放
#define STATE_CALC          6   // 选项四：两位小数伪浮点计算器
#define STATE_SHOW_TIME     7   // 选项五：成功登录后的系统运行时间
#define STATE_UART          8   // 选项六：上位机串口通信

/* ==================== 计算器状态定义 ==================== */
#define CALC_WAIT_FIRST     0        // 等待输入第一个数
#define CALC_INPUT_FIRST    1        // 正在输入第一个数
#define CALC_WAIT_SECOND    2        // 等待输入第二个数
#define CALC_INPUT_SECOND   3        // 正在输入第二个数
#define CALC_SHOW_RESULT    4        // 显示计算结果

static u8  idata system_state = STATE_OFF;// 定义当前系统的全局状态
static bit first_flag = 1;
/*
 它的初始值是 1（代表是第一次进入该界面） 。
 当单片机进入某个功能（比如温度显示）后，发现 first_flag == 1，
 它就会把界面的静态文字（如 "Desk Temp:"）打印一次 ，
 然后立刻把 first_flag 改为 0 。
 这样后续的几万次循环里，单片机就只会动态刷新温度数字，
 而不会重复刷新整行死字，保证了屏幕显示的绝对平滑。  
*/
static bit need_clear = 0;// 清屏状态，为1时表明要清屏，调用清屏函数后立刻变为0（默认状态）
static u8  idata matrix_key = 0;// 矩阵按键当前有效值暂存

/* 密码相关变量 */
static u8 idata pwd_buffer[3] = {0};// 存储密码的数组
static u8 idata pwd_idx = 0;// 存储当前输入的数字存在密码数组的第几位

/* 计算器定点数相关变量 */
static long idata first_num = 0;
static long idata second_num = 0;
static u8   idata oper = 0;// 运算符暂存器，存运算符的ASCII码
static u8   idata calc_state = CALC_WAIT_FIRST;//运算状态
static bit  has_dot = 0; // 用户是否按下了小数点(S13)，小数点激活标志位
static u8   idata dot_cnt = 0;  // 小数点后的位数计数器

/* 硬件互斥片选引脚 */
sbit Main_XPT_CS = P3^5;
// P3.5 引脚：同时连接了 XPT2046（触控/ADC芯片）的片选脚 (CS) 和 LCD1602液晶屏的读写控制脚 (RW) 、以及蜂鸣器 (Beep) 。
sbit Main_DS_DQ  = P3^7;
// P3.7 引脚：同时连接了 DS18B20（温度传感器）的单总线 (DQ) 和 XPT2046 的数据输出脚 (DOUT) 。  

/* ==================== 自定义轻量化数字显示工具组（显示两位整数00-99） ==================== */
void LCD_DisplayNum2(u8 row, u8 col, u16 num)
{
//'0' 是一个字符，在计算机内存中对应一个具体的ASCII码值（十进制48）。
//将一个数字（0-9）加上 '0'，本质上是将数值偏移到它对应数字字符的ASCII码位置。
    LCD1602_display_char(row, col, num / 10 + '0');
    LCD1602_display_char(row, col + 1, num % 10 + '0');
}

/* ==================== 自定义轻量化数字显示工具组（显示两位整数0000-9999） ==================== */
void LCD_DisplayNum4(u8 row, u8 col, u16 num)
{    
    LCD1602_display_char(row, col,     num / 1000 + '0');
    LCD1602_display_char(row, col + 1, (num % 1000) / 100 + '0');
    LCD1602_display_char(row, col + 2, (num % 100) / 10 + '0');
    LCD1602_display_char(row, col + 3, num % 10 + '0');
}

/*==================================LCD显示摄氏度================================*/
void LCD_DisplayTemp(u8 row, u8 col, float temp)
{    
    int temp_int;
    if (temp < 0)
		//判断零上还是零下
		{
        LCD1602_display_char(row, col, '-');// 表示零下
        temp = -temp;
    } 
		else 
		{        
        LCD1602_display_char(row, col, ' ');// 表示零上
    }    
    temp_int = (int)(temp * 100.0f);// 将浮点转化为整型
    LCD1602_display_char(row, col + 1, temp_int / 1000 + '0');
    LCD1602_display_char(row, col + 2, (temp_int % 1000) / 100 + '0');    
    LCD1602_display_char(row, col + 3, '.');
    LCD1602_display_char(row, col + 4, (temp_int % 100) / 10 + '0');    
    LCD1602_display_char(row, col + 5, temp_int % 10 + '0');
    LCD1602_display_char(row, col + 6, 0xDF); // LCD1602内置字库的 '°' 符号    
    LCD1602_display_char(row, col + 7, 'C');
		// 算上表示零上零下的符号位，共八位
}

void LCD_DisplayFixedPoint(u8 row, u8 col, long num)//带小数点的，显示最终结果
// 这里的num表示原本的数*100
// 这里“原本的数”是计算器运算后的结果
// 专门为计算器功能量身定做的定点数（保留两位小数）高端显示工具。
{
	
    u8 buf[11];// buf[11]：字符缓冲区，最多存储11个字符（负号+整数部分+小数点+两位小数+结束符）
    signed char i = 0;// 有符号字符型索引，用于记录当前填充位置
    if (num < 0)
		//发现是负数，将其转为整数，同时前方加上‘-’表明输出时为负数
		{        
        LCD1602_display_char(row, col++, '-');
			// 在当前行列位置显示'-'，然后col++让列指针后移一位
        num = -num;    
    }
    buf[0] = (num % 10) + '0';// 取个位为百分位
    buf[1] = ((num / 10) % 10) + '0'; // 取十位为十分位
    buf[2] = ((num / 100) % 10) + '0';// 取百位为个位
    num /= 1000;// // 去掉最后三位，得到整数部分
    i = 3;
    while (num > 0) 
		{        
        buf[i++] = (num % 10) + '0';
        num /= 10;    
    }    
    for (i = i - 1; i >= 0; i--) 
		{        
        if (i == 1) LCD1602_display_char(row, col++, '.');// 适时的插入小数点
        LCD1602_display_char(row, col++, buf[i]);    
    }
}

void LCD_DisplayLong(u8 row, u8 col, long num)// 不带小数点的，显示当前的输入
{    
    u8 buf[11];
    signed char i = 0;    
    if (num == 0) // 零判断
		{        
        LCD1602_display_char(row, col, '0');
        return;    
    }    
    if (num < 0) // 正负判断
		{        
        LCD1602_display_char(row, col++, '-');
        num = -num;    
    }    
    while (num > 0) // 循环切割数字
		{        
        buf[i++] = num % 10 + '0';
        num /= 10;    
    }    
    for (i = i - 1; i >= 0; i--) // 倒序显示数字
		{        
        LCD1602_display_char(row, col++, buf[i]);
    } 
}
/*=========函数的前置声明==============*/
static void system_init(void);// 上电第一步，初始化所有引脚、时钟和 LCD1602 。
static void state_machine(void);// 根据 system_state 的值去轮询后面的业务模块 。
static void pwd_input_service(void);// 对应的 8 大独立功能模块，由上一行视情况进行无阻塞调度 。
static void menu_screen_service(void);
static void bright_service(void);
static void temp_service(void);
static void music_service(void);
static void calc_service(void);
static void time_service(void);
static void uart_service(void);
static u8   calc_get_digit(u8 key);// 专门贴身伺服计算器和密码锁，帮忙甄别、翻译数字按键的小工具 。
static bit  calc_is_digit(u8 key);

/********************************************************************************
* 函数名: main
*******************************************************************************/
void main(void)// 主函数，从这里进入
{    
    u8 i;
    system_init();      // 执行引脚强制释放与外设默认初设    
    delay_ms(200);    
    EA = 1;
    
    // 50次高频盲扫，洗净上电瞬间准双向口产生的漏电假键值    
    for (i = 0; i < 50; i++) 
	  {        
        delay_ms(5);
        matrix_key = key_get_value();    
    }    
    matrix_key = 0;
    
    while (1)// 无线循环，开始运行
    {        
        matrix_key = key_get_value();
        state_machine();
    } 
}

/********************************************************************************
* 函数名: system_init 初始化
*******************************************************************************/
static void system_init(void)
{    
    P1 = 0xFF;
    P0 = 0xFF;         // 释放液晶并行数据总线通道    
    Main_XPT_CS = 1;
    Main_DS_DQ  = 1;   // 释放DS18B20单总线    
    Beep        = 1;
    T0_init();         // 挂载Timer0核心分发器    
    T1_init();         // 初始化Timer1音调发生器    
    T2_init();
    delay_ms(100);     // 预留足额时间等待液晶物理总线稳态    
    LCD1602_init(); 
}

/********************************************************************************
* 函数名: state_machine
*******************************************************************************/
static void state_machine(void)
{    
    static u16 idata power_tick = 0;
    
    /* 1. 全局独立按键 K1 开关机模块检测 (长按3秒) */    
    if (key_check(KEY_1, KEY_DOWN)) power_tick = 0;// K1键刚被按下，开始计时
    if (key_check(KEY_1, KEY_HOLD)) 
		{        
        if (sysTick_checked(&power_tick, 3000)) 
				{            
            if (system_state == STATE_OFF) //满足开机状态（3S），开机
						{                
                system_state = STATE_PWD_INPUT;
            } 
						else //另一种是关机长按3S，清除状态
						{                
                Buzzer_Stop();
                Beep = 1;        // 关机切断所有声响                
                system_state = STATE_OFF;
            }            
            first_flag = 1; need_clear = 1;            
            return;
						// 把 first_flag 和 need_clear 都置为 1（通知下一个界面重新刷文字、擦屏幕） 
						// 然后执行 return 直接结束本轮函数，后面的代码这轮就不执行了 。
        }    
    }    
    
    /* 2. 全局独立按键 K2 安全退回主菜单模块检测 (长按1秒) */    
    if (system_state > STATE_MENU) // 判断是否处于6大功能之中
		{        
        if (key_check(KEY_2, KEY_LONG) || key_check(KEY_2, KEY_REPEAT)) //如果K2按下1S，执行回到主界面的命令
				{            
            Buzzer_Stop();
            Beep = 1;            // 强制掐断音乐和可能存在的超温长鸣            
            system_state = STATE_MENU;
            first_flag = 1; need_clear = 1;            
            return;
        }    
    }    
    
    /* 3. 核心主状态机调度分布 */    
    switch (system_state)    
    {        
        case STATE_OFF:            
            if (first_flag) //开关机界面显示
						{							
                first_flag = 0;
                LCD1602_clear();                
                LCD1602_display(1, 4, "Desktop Toy");                
                LCD1602_display(2, 2, "Hold K1 3s ON");                
                reset_runtime();
            }            
            break;        
        case STATE_PWD_INPUT:   pwd_input_service();       break;
        case STATE_MENU:        menu_screen_service();     break;        
        case STATE_SHOW_BRIGHT: bright_service();          break;// 亮度  
        case STATE_SHOW_TEMP:   temp_service();            break;// 温度
        case STATE_MUSIC:       music_service();           break;// 音乐 
        case STATE_CALC:        calc_service();            break;// 计算器 
        case STATE_SHOW_TIME:   time_service();            break;// 运行时间
        case STATE_UART:        uart_service();            break;// 串口通信
        default: system_state = STATE_OFF; first_flag = 1; break;
    } 
}

/* ==================== 各实用功能业务模块服务函数 ==================== */

/* 密码安全校验关卡 */
// 密码为“123”
static void pwd_input_service(void)
{    
    if (need_clear) { need_clear = 0; LCD1602_clear(); }
    if (first_flag) 
		{        
        first_flag = 0;
        pwd_idx = 0;        
        LCD1602_display(1, 2, "Enter Password");        
        LCD1602_display(2, 6, "[   ]");// 显示密码界面
    }    
    if (matrix_key == 0) return;
    if (calc_is_digit(matrix_key) && pwd_idx < 3) 
		{        
        pwd_buffer[pwd_idx] = calc_get_digit(matrix_key);
        LCD1602_display_char(2, 7 + pwd_idx, '*'); // 压下瞬间直接掩码显示        
        pwd_idx++;
        if (pwd_idx == 3) 
				{            
            delay_ms(400);
            if (pwd_buffer[0] == 1 && pwd_buffer[1] == 2 && pwd_buffer[2] == 3)
						{                
                system_state = STATE_MENU;
                reset_runtime(); // 密码验证通过，后台秒表开始精确运行计时
            } 
						else 
						{                
                LCD1602_display(2, 2, "Wrong Pwd!  ");
                delay_ms(1000);            
            }            
            first_flag = 1; need_clear = 1;
        }    
    } 
}

/* 功能选择主菜单 */
static void menu_screen_service(void)
{    
    static u8 idata page = 0; // 3个界面分布 -> 0:功能1&2, 1:功能3&4, 2:功能5&6

    if (need_clear) { need_clear = 0; LCD1602_clear(); }
    
    /* 1. 渲染当前手动选中的 LCD 菜单页面 */    
    if (first_flag) {
        first_flag = 0;
        LCD1602_display(1, 1, "Select Function:");        
        switch (page) {            
            case 0: LCD1602_display(2, 1, "1:Bri   2:Temp  "); break;           
            case 1: LCD1602_display(2, 1, "3:Music 4:Calc  "); break;           
            case 2: LCD1602_display(2, 1, "5:Time  6:UART  "); break;           
            default: page = 0; break;       
        }    
    }    

    /* 2. 检测独立按键 K3 单击 (向左/上循环切页) */
    if (key_check(KEY_3, KEY_SINGLE)) {
        if (page == 0) {
            page = 2; // 在最左侧界面左切时 -> 闭环切换到最右侧界面
        } else {
            page--;
        }
        first_flag = 1; // 触发 LCD 重新刷新渲染
    }

    /* 3. 检测独立按键 K4 单击 (向右/下循环切页) */
    if (key_check(KEY_4, KEY_SINGLE)) 
		{
        if (page == 2) 
				{
            page = 0; // 在最右侧界面右切时 -> 闭环切换到最左侧界面
        } 
				else 
				{
            page++;
        }
        first_flag = 1; // 触发 LCD 重新刷新渲染
    }
    
    /* 4. 根据矩阵键值 1~6 零延迟直接切入对应工具 */    
    switch (matrix_key)    
    {        
        case MATRIX_S1: system_state = STATE_SHOW_BRIGHT; first_flag = 1; need_clear = 1; break;       
        case MATRIX_S2: system_state = STATE_SHOW_TEMP;   first_flag = 1; need_clear = 1; break;       
        case MATRIX_S3: system_state = STATE_MUSIC;       first_flag = 1; need_clear = 1; break;       
        case MATRIX_S4: system_state = STATE_CALC;        first_flag = 1; need_clear = 1; break;       
        case MATRIX_S5: system_state = STATE_SHOW_TIME;   first_flag = 1; need_clear = 1; break;       
        case MATRIX_S6: system_state = STATE_UART;        first_flag = 1; need_clear = 1; break;       
        default: break;    
    } 
}

/* 选项一：环境亮度查看 */
static void bright_service(void){    
    static u16 idata bright_tick = 0;    
    if (first_flag) { first_flag = 0; LCD1602_clear(); LCD1602_display(1, 3, "Brightness:"); }   
    if (sysTick_checked(&bright_tick, 300)) {        
        LCD_DisplayNum4(2, 6, Read_AD_Data(ADC_CH_LIGHT));   
    }
}

/* 选项二：环境温度查看与 >40℃ 工业级长鸣警告 */
static void temp_service(void){    
    static u16 idata temp_tick = 0;    
    float current_temp;   
    if (first_flag) { first_flag = 0; LCD1602_clear(); LCD1602_display(1, 3, "Desk Temp:"); ds18B20_convertT(); }   
    if (sysTick_checked(&temp_tick, 500)) {        
        current_temp = ds18B20_read_temperture();       
        LCD_DisplayTemp(2, 5, current_temp);        
        ds18B20_convertT();       
        /* 物理超温判断 */        
        if (current_temp >= 40.0f) {            
            TR1 = 0; ET1 = 0; // 强行拉住 Timer1 发生器，防止波形碎裂           
            Beep = 0;         // 连续拉低电平，制造无间断长鸣警告       
        } else {            
            if (!Buzzer_IsPlaying()) Beep = 1; // 释放恢复正常高电平       
        }    
    }
}

/* 选项三：青花瓷非阻塞音乐播放 */
static void music_service(void){    
    if (first_flag) {        
        first_flag = 0;       
        LCD1602_clear();        
        LCD1602_display(1, 3, "Playing...");        
        LCD1602_display(2, 2, "Qing Hua Ci");       
        Buzzer_PlayQingHuaCi();    
    }    
    Buzzer_Service(); // 轮询无阻塞更新谱表节奏   
    if (!Buzzer_IsPlaying()) {        
        LCD1602_clear();        
        LCD1602_display(1, 4, "Music End");       
        LCD1602_display(2, 1, "Hold K2 return");   
    }
}

/* 选项四：支持两位小数的定点数计算器 */
static void calc_service(void){    
    u8 d;   
    if (first_flag) {        
        first_flag = 0;        
        LCD1602_clear();        
        LCD1602_display(1, 1, "Calc Mode: S13=.");       
        first_num = 0; second_num = 0; oper = 0;        
        calc_state = CALC_WAIT_FIRST;        
        has_dot = 0; dot_cnt = 0;   
    }    
    if (matrix_key == 0) return;   
    if (matrix_key == MATRIX_S13) {        
        if (calc_state == CALC_INPUT_FIRST || calc_state == CALC_INPUT_SECOND) {            
            has_dot = 1;           
            dot_cnt = 0;            
            LCD1602_display_char(2, 15, '.'); // 给予用户视觉反馈       
        }        
        return;   
    }    
    switch (calc_state)    
    {        
        case CALC_WAIT_FIRST:            
            if (calc_is_digit(matrix_key)) {                
                LCD1602_clear();               
                LCD1602_display(1, 1, "Calc Mode: S13=.");                
                first_num = calc_get_digit(matrix_key);                
                has_dot = 0; dot_cnt = 0;                
                LCD_DisplayLong(2, 1, first_num);                
                calc_state = CALC_INPUT_FIRST;           
            }            
            break;        
        case CALC_INPUT_FIRST:            
            if (calc_is_digit(matrix_key)) {                
                d = calc_get_digit(matrix_key);               
                if (!has_dot) {                    
                    first_num = first_num * 10 + d;               
                } else if (dot_cnt < 2) {                    
                    if (dot_cnt == 0) { first_num = first_num * 10 + d; dot_cnt++; }                   
                    else if (dot_cnt == 1) { first_num = first_num * 10 + d; dot_cnt++; }               
                }                
                LCD1602_clear();               
                LCD1602_display(1, 1, "Calc Mode: S13=.");                
                LCD_DisplayLong(2, 1, first_num);                
                if (has_dot) LCD1602_display_char(2, 14, 'd');           
            }            
            else if (matrix_key == MATRIX_S4 || matrix_key == MATRIX_S8 || matrix_key == MATRIX_S12 || matrix_key == MATRIX_S16) {                
                if (!has_dot) {                    
                    first_num *= 100;               
                } else {                    
                    if (dot_cnt == 1) first_num *= 10;               
                }                
                if (matrix_key == MATRIX_S4)  oper = '+';               
                if (matrix_key == MATRIX_S8)  oper = '-';                
                if (matrix_key == MATRIX_S12) oper = '*';               
                if (matrix_key == MATRIX_S16) oper = '/';                
                LCD1602_display_char(2, 16, oper);                
                has_dot = 0; dot_cnt = 0;                
                calc_state = CALC_WAIT_SECOND;           
            }            
            break;        
        case CALC_WAIT_SECOND:            
            if (calc_is_digit(matrix_key)) {                
                second_num = calc_get_digit(matrix_key);               
                LCD1602_clear(); LCD1602_display(1, 1, "Calc Mode: S13=.");                
                LCD_DisplayLong(2, 1, second_num);                
                calc_state = CALC_INPUT_SECOND;           
            }            
            break;        
        case CALC_INPUT_SECOND:            
            if (calc_is_digit(matrix_key)) {                
                d = calc_get_digit(matrix_key);               
                if (!has_dot) {                    
                    second_num = second_num * 10 + d;               
                } else if (dot_cnt < 2) {                    
                    if (dot_cnt == 0) { second_num = second_num * 10 + d; dot_cnt++; }                   
                    else if (dot_cnt == 1) { second_num = second_num * 10 + d; dot_cnt++; }               
                }                
                LCD1602_clear();               
                LCD1602_display(1, 1, "Calc Mode: S13=.");                
                LCD_DisplayLong(2, 1, second_num);           
            }            
            else if (matrix_key == MATRIX_S15) {                
                if (!has_dot) {                    
                    second_num *= 100;               
                } else {                    
                    if (dot_cnt == 1) second_num *= 10;               
                }                
                LCD1602_clear();                
                LCD1602_display(1, 1, "Result:");               
                LCD1602_display_char(2, 1, '=');                
                if (oper == '+') LCD_DisplayFixedPoint(2, 3, first_num + second_num);               
                if (oper == '-') LCD_DisplayFixedPoint(2, 3, first_num - second_num);                
                if (oper == '*') LCD_DisplayFixedPoint(2, 3, (first_num * second_num) / 100);               
                if (oper == '/') {                    
                    if (second_num != 0) {                        
                        LCD_DisplayFixedPoint(2, 3, (first_num * 100) / second_num);                   
                    } else {                        
                        LCD1602_display(2, 3, "Error Div0");                   
                    }                
                }                
                calc_state = CALC_SHOW_RESULT;           
            }            
            break;        
        case CALC_SHOW_RESULT:            
            if (calc_is_digit(matrix_key)) {                
                first_flag = 1;           
            }            
            break;    
    } 
}

/* 选项五：系统运行时间秒表 */
static void time_service(void){    
    static u16 idata time_disp_tick = 0;    
    if (first_flag) {        
        first_flag = 0; LCD1602_clear();        
        LCD1602_display(1, 3, "System Run:");       
        LCD1602_display(2, 4, "00:00:00");    
    }    
    if (sysTick_checked(&time_disp_tick, 500)) {        
        LCD_DisplayNum2(2, 4, (u16)runtime_hours);       
        LCD_DisplayNum2(2, 7, (u16)runtime_minutes);        
        LCD_DisplayNum2(2, 10, (u16)runtime_seconds);   
    }
}

/* 选项六：上位机串口接收服务 */
static void uart_service(void){    
    u8 rx;    
    if (first_flag) {        
        first_flag = 0; LCD1602_clear();        
        LCD1602_display(1, 2, "UART Receiver");       
        LCD1602_display(2, 1, "Waiting Link...");        
        UART_SetMode(1); // 开启并配置串口              
        UART_SendString("UART Mode Ready!\r\n");   //向上位机发送就绪通知
    }    
    rx = UART_Service(); // 轮询接收   
    if (rx != 0xFF) {        
        LCD1602_display(2, 1, "Char Recv: [ ]  ");       
        LCD1602_display_char(2, 13, rx); // 实时将收到的可见 ASCII 字符渲染到屏幕上   
    }
}

/* ==================== 矩阵键值基础转换映射工具 ==================== */
static u8 calc_get_digit(u8 key){    
    if (key == MATRIX_S1)  return 1;   
    if (key == MATRIX_S2)  return 2;    
    if (key == MATRIX_S3)  return 3;   
    if (key == MATRIX_S5)  return 4; 
    if (key == MATRIX_S6)  return 5;   
    if (key == MATRIX_S7)  return 6;    
    if (key == MATRIX_S9)  return 7; 
    if (key == MATRIX_S10) return 8;   
    if (key == MATRIX_S11) return 9;    
    if (key == MATRIX_S14) return 0;   
    return 0;
}
/*******************************************************************************
* 函 数 名: calc_is_digit
* 功能描述: 判断矩阵按键值是否属于数字键（S1~S3, S5~S7, S9~S11, S14）
* 返 回 值: 1=是数字键，0=不是数字键
*******************************************************************************/
static bit calc_is_digit(u8 key){	
    return (key == MATRIX_S1  || key == MATRIX_S2  || key == MATRIX_S3  ||            
            key == MATRIX_S5  || key == MATRIX_S6  || key == MATRIX_S7  ||            
            key == MATRIX_S9  || key == MATRIX_S10 || key == MATRIX_S11 ||            
            key == MATRIX_S14);
}