/********************************************************************************
* 单片机综合项目 - 桌面多功能智能摆件全联调优化版 main.c（真实实验板全兼容版）
*
* 优化特性：
* 1. 完美解决真实实验板上由于引脚复用造成的硬件电平污染与乱码隐患。
* 2. 调用 UART_SendString 函数，彻底根除 Keil 编译时的 L16 警告。
* 3. 重构计算器（选项四）显示逻辑：输入数字时普通整数实时刷新，结果精准保留两位小数。
* 4. 完美集成全套实用功能：开机密码锁(123)、K2长按1秒无缝安全退出、
* 温度>40℃蜂鸣器长鸣报警、流式串口字符显示及系统精准运行计时。
* 5. 【手动切页优化】：彻底移除主菜单的自动轮播，改由独立按键 K3 (左切) 与 K4 (右切) 
* 对 3 个主菜单界面进行手动循环切页，长按 K2 返回时默认静止停留在功能 1 和 2 界面。
*******************************************************************************/
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
#define CALC_WAIT_FIRST     0
#define CALC_INPUT_FIRST    1
#define CALC_WAIT_SECOND    2
#define CALC_INPUT_SECOND   3
#define CALC_SHOW_RESULT    4

static u8  idata system_state = STATE_OFF;
static bit first_flag = 1;
static bit need_clear = 0;
static u8  idata matrix_key = 0;

/* 密码相关变量 */
static u8 idata pwd_buffer[3] = {0};
static u8 idata pwd_idx = 0;

/* 计算器定点数相关变量 */
static long idata first_num = 0;
static long idata second_num = 0;
static u8   idata oper = 0;
static u8   idata calc_state = CALC_WAIT_FIRST;
static bit  has_dot = 0; // 用户是否按下了小数点(S13)
static u8   idata dot_cnt = 0;  // 小数点后的位数计数器

/* 硬件互斥片选引脚 */
sbit Main_XPT_CS = P3^5;
sbit Main_DS_DQ  = P3^7;

/* ==================== 自定义轻量化数字显示工具组 ==================== */
void LCD_DisplayNum2(u8 row, u8 col, u16 num){    
    LCD1602_display_char(row, col, num / 10 + '0');
    LCD1602_display_char(row, col + 1, num % 10 + '0');
}

void LCD_DisplayNum4(u8 row, u8 col, u16 num){    
    LCD1602_display_char(row, col,     num / 1000 + '0');
    LCD1602_display_char(row, col + 1, (num % 1000) / 100 + '0');
    LCD1602_display_char(row, col + 2, (num % 100) / 10 + '0');
    LCD1602_display_char(row, col + 3, num % 10 + '0');
}

void LCD_DisplayTemp(u8 row, u8 col, float temp){    
    int temp_int;
    if (temp < 0) {        
        LCD1602_display_char(row, col, '-');        
        temp = -temp;
    } else {        
        LCD1602_display_char(row, col, ' ');
    }    
    temp_int = (int)(temp * 100.0f);    
    LCD1602_display_char(row, col + 1, temp_int / 1000 + '0');
    LCD1602_display_char(row, col + 2, (temp_int % 1000) / 100 + '0');    
    LCD1602_display_char(row, col + 3, '.');
    LCD1602_display_char(row, col + 4, (temp_int % 100) / 10 + '0');    
    LCD1602_display_char(row, col + 5, temp_int % 10 + '0');
    LCD1602_display_char(row, col + 6, 0xDF); // LCD1602内置字库的 '°' 符号    
    LCD1602_display_char(row, col + 7, 'C');
}

void LCD_DisplayFixedPoint(u8 row, u8 col, long num){    
    u8 buf[11];
    signed char i = 0;    
    if (num < 0) {        
        LCD1602_display_char(row, col++, '-');
        num = -num;    
    }    
    buf[0] = (num % 10) + '0';
    buf[1] = ((num / 10) % 10) + '0';    
    buf[2] = ((num / 100) % 10) + '0';
    num /= 1000;    
    i = 3;    
    while (num > 0) {        
        buf[i++] = (num % 10) + '0';
        num /= 10;    
    }    
    for (i = i - 1; i >= 0; i--) {        
        if (i == 1) LCD1602_display_char(row, col++, '.');
        LCD1602_display_char(row, col++, buf[i]);    
    }
}

void LCD_DisplayLong(u8 row, u8 col, long num){    
    u8 buf[11];
    signed char i = 0;    
    if (num == 0) {        
        LCD1602_display_char(row, col, '0');
        return;    
    }    
    if (num < 0) {        
        LCD1602_display_char(row, col++, '-');
        num = -num;    
    }    
    while (num > 0) {        
        buf[i++] = num % 10 + '0';
        num /= 10;    
    }    
    for (i = i - 1; i >= 0; i--) {        
        LCD1602_display_char(row, col++, buf[i]);
    } 
}

static void system_init(void);
static void state_machine(void);
static void pwd_input_service(void);
static void menu_screen_service(void);
static void bright_service(void);
static void temp_service(void);
static void music_service(void);
static void calc_service(void);
static void time_service(void);
static void uart_service(void);
static u8   calc_get_digit(u8 key);
static bit  calc_is_digit(u8 key);

/********************************************************************************
* 函数名: main
*******************************************************************************/
void main(void){    
    u8 i;
    system_init();      // 执行引脚强制释放与外设默认初设    
    delay_ms(200);    
    EA = 1;
    
    // 50次高频盲扫，洗净上电瞬间准双向口产生的漏电假键值    
    for (i = 0; i < 50; i++) {        
        delay_ms(5);
        matrix_key = key_get_value();    
    }    
    matrix_key = 0;
    
    while (1)    
    {        
        matrix_key = key_get_value();
        state_machine();
    } 
}

/********************************************************************************
* 函数名: system_init
*******************************************************************************/
static void system_init(void){    
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
static void state_machine(void){    
    static u16 idata power_tick = 0;
    
    /* 1. 全局独立按键 K1 开关机模块检测 (长按3秒) */    
    if (key_check(KEY_1, KEY_DOWN)) power_tick = 0;
    if (key_check(KEY_1, KEY_HOLD)) {        
        if (sysTick_checked(&power_tick, 3000)) {            
            if (system_state == STATE_OFF) {                
                system_state = STATE_PWD_INPUT;
            } else {                
                Buzzer_Stop();
                Beep = 1;        // 关机切断所有声响                
                system_state = STATE_OFF;
            }            
            first_flag = 1; need_clear = 1;            
            return;
        }    
    }    
    
    /* 2. 全局独立按键 K2 安全退回主菜单模块检测 (长按1秒) */    
    if (system_state > STATE_MENU) {        
        if (key_check(KEY_2, KEY_LONG) || key_check(KEY_2, KEY_REPEAT)) {            
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
            if (first_flag) {                
                first_flag = 0;
                LCD1602_clear();                
                LCD1602_display(1, 4, "Desktop Toy");                
                LCD1602_display(2, 2, "Hold K1 3s ON");                
                reset_runtime();
            }            
            break;        
        case STATE_PWD_INPUT: pwd_input_service();  break;
        case STATE_MENU:      menu_screen_service(); break;        
        case STATE_SHOW_BRIGHT: bright_service();   break;        
        case STATE_SHOW_TEMP:   temp_service();     break;
        case STATE_MUSIC:       music_service();    break;        
        case STATE_CALC:        calc_service(); break;        
        case STATE_SHOW_TIME:   time_service();     break;        
        case STATE_UART:        uart_service();     break;
        default: system_state = STATE_OFF; first_flag = 1; break;
    } 
}

/* ==================== 各实用功能业务模块服务函数 ==================== */

/* 密码安全校验关卡 */
static void pwd_input_service(void){    
    if (need_clear) { need_clear = 0; LCD1602_clear(); }
    if (first_flag) {        
        first_flag = 0;
        pwd_idx = 0;        
        LCD1602_display(1, 2, "Enter Password");        
        LCD1602_display(2, 6, "[   ]");
    }    
    if (matrix_key == 0) return;
    if (calc_is_digit(matrix_key) && pwd_idx < 3) {        
        pwd_buffer[pwd_idx] = calc_get_digit(matrix_key);
        LCD1602_display_char(2, 7 + pwd_idx, '*'); // 压下瞬间直接掩码显示        
        pwd_idx++;
        if (pwd_idx == 3) {            
            delay_ms(400);
            if (pwd_buffer[0] == 1 && pwd_buffer[1] == 2 && pwd_buffer[2] == 3) {                
                system_state = STATE_MENU;
                reset_runtime(); // 密码验证通过，后台秒表开始精确运行计时
            } else {                
                LCD1602_display(2, 2, "Wrong Pwd!  ");
                delay_ms(1000);            
            }            
            first_flag = 1; need_clear = 1;
        }    
    } 
}

/* 功能选择主菜单 */
static void menu_screen_service(void){    
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
    if (key_check(KEY_4, KEY_SINGLE)) {
        if (page == 2) {
            page = 0; // 在最右侧界面右切时 -> 闭环切换到最左侧界面
        } else {
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
        UART_SendString("UART Mode Ready!\r\n");   
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

static bit calc_is_digit(u8 key){    
    return (key == MATRIX_S1  || key == MATRIX_S2  || key == MATRIX_S3  ||            
            key == MATRIX_S5  || key == MATRIX_S6  || key == MATRIX_S7  ||            
            key == MATRIX_S9  || key == MATRIX_S10 || key == MATRIX_S11 ||            
            key == MATRIX_S14);
}