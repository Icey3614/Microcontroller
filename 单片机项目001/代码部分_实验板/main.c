/********************************************************************************
* 单片机综合项目 - 桌面多功能智能摆件全联调优化版 main.c（真实实验板全兼容版）
*
* 【V2.0 优化版说明】
* 1. 全整数定点化：DS18B20 温度读取与显示彻底移除 float 浮点运算（改为 ×100 定点整数），
*    根除 Keil C51 浮点库带来的代码体积开销，为新增功能腾出 Flash 空间。
* 2. LCD 增量刷新：底层新增 LCD1602_Position / LCD1602_clear_line 批量写接口，
*    计算器、串口、倒计时等界面只刷新变化区域，不再整屏清空重绘，更快且无闪烁。
* 3. 计算器重构：去除三处重复清屏重绘，合并 dot_cnt 冗余分支，数字键映射改查表；
*    修复小数点占位显示错误（原代码误显示字母 'd'）。
* 4. 修复音乐结束提示死代码：曲谱设计为循环播放，原“Music End”分支永不触发，已移除。
* 5. 菜单扩容：由 3 页 6 功能扩展为 5 页 9 功能（K3/K4 循环切页，S1~S9 直达）。
* 6. 新增功能七：电子琴（S1~S16 对应 16 个音阶，Timer1 发声，250ms 自动停音）；
*    新增功能八：倒计时器（4 位数字 MM:SS 设定，K4 启停、K3 清零，时间到蜂鸣长鸣）；
*    新增功能九：反应速度测试（随机延时后显示 GO，按键后测量反应毫秒数）。
* 7. 保留全部原有功能与硬件隔离策略（P2.5 / P3.7 复用隔离、50 次上电盲扫等）。
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

/* ==================== 全局状态定义 ==================== */
#define STATE_OFF           0   // 关机状态
#define STATE_PWD_INPUT     1   // 密码输入状态
#define STATE_MENU          2   // 功能选择主菜单（对应原 STATE_MAIN）
#define STATE_SHOW_BRIGHT   3   // 选项一：环境亮度
#define STATE_SHOW_TEMP     4   // 选项二：环境温度（带40℃报警）
#define STATE_MUSIC         5   // 选项三：青花瓷音乐播放
#define STATE_CALC          6   // 选项四：两位小数定点计算器
#define STATE_SHOW_TIME     7   // 选项五：系统运行时间
#define STATE_UART          8   // 选项六：上位机串口通信
#define STATE_PIANO         9   // 选项七：电子琴
#define STATE_COUNTDOWN     10  // 选项八：倒计时器
#define STATE_REACT         11  // 选项九：反应速度测试

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
/* 说明：以下显示函数均改为“一次定位 + 连续写数据”，
   避免每个字符重复发送地址指令，显著减少 LCD 刷新耗时。 */
void LCD_DisplayNum2(u8 row, u8 col, u16 num){
    LCD1602_Position(row, col);
    LCD1602_write_data((u8)(num / 10) + '0');
    LCD1602_write_data((u8)(num % 10) + '0');
}

void LCD_DisplayNum4(u8 row, u8 col, u16 num){
    LCD1602_Position(row, col);
    LCD1602_write_data((u8)(num / 1000) + '0');
    LCD1602_write_data((u8)((num % 1000) / 100) + '0');
    LCD1602_write_data((u8)((num % 100) / 10) + '0');
    LCD1602_write_data((u8)(num % 10) + '0');
}

/* 温度显示：temp_c100 为温度×100 的定点整数（如 25.50℃ → 2550），
   全程无浮点运算，显示格式为 "XX.XX°C"。 */
void LCD_DisplayTemp(u8 row, u8 col, int temp_c100){
    u8 neg = 0;
    if (temp_c100 < 0) { neg = 1; temp_c100 = -temp_c100; }
    if (temp_c100 > 9999) temp_c100 = 9999;   // 超过 99.99℃ 时封顶显示，防止溢出错位
    LCD1602_Position(row, col);
    LCD1602_write_data(neg ? '-' : ' ');
    LCD1602_write_data((u8)((temp_c100 / 1000) % 10) + '0');
    LCD1602_write_data((u8)((temp_c100 / 100) % 10) + '0');
    LCD1602_write_data('.');
    LCD1602_write_data((u8)((temp_c100 / 10) % 10) + '0');
    LCD1602_write_data((u8)(temp_c100 % 10) + '0');
    LCD1602_write_data(0xDF); // LCD1602内置字库的 '°' 符号
    LCD1602_write_data('C');
}

void LCD_DisplayFixedPoint(u8 row, u8 col, long num){
    u8 buf[11];
    u8 i = 0;
    if (num < 0) {
        LCD1602_display_char(row, col++, '-');
        num = -num;
    }
    buf[i++] = (u8)(num % 10) + '0';        // 百分位
    buf[i++] = (u8)((num / 10) % 10) + '0'; // 十分位
    buf[i++] = (u8)((num / 100) % 10) + '0';// 个位
    num /= 1000;
    while (num > 0) {
        buf[i++] = (u8)(num % 10) + '0';
        num /= 10;
    }
    LCD1602_Position(row, col);
    while (i > 0) {
        if (i == 2) LCD1602_write_data('.'); // 倒序打印到十分位前插入小数点
        LCD1602_write_data(buf[--i]);
    }
}

void LCD_DisplayLong(u8 row, u8 col, long num){
    u8 buf[11];
    u8 i = 0;
    if (num == 0) {
        LCD1602_display_char(row, col, '0');
        return;
    }
    if (num < 0) {
        LCD1602_display_char(row, col++, '-');
        num = -num;
    }
    while (num > 0) {
        buf[i++] = (u8)(num % 10) + '0';
        num /= 10;
    }
    LCD1602_Position(row, col);
    while (i > 0) {
        LCD1602_write_data(buf[--i]);
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
static void piano_service(void);
static void countdown_service(void);
static void react_service(void);
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
        case STATE_CALC:        calc_service();     break;
        case STATE_SHOW_TIME:   time_service();     break;
        case STATE_UART:        uart_service();     break;
        case STATE_PIANO:       piano_service();    break;
        case STATE_COUNTDOWN:   countdown_service(); break;
        case STATE_REACT:       react_service();    break;
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

/* 功能选择主菜单（5 页 9 功能，K3/K4 循环切页，S1~S9 直达） */
static void menu_screen_service(void){
    static u8 idata page = 0; // 5个界面 -> 0:(1,2) 1:(3,4) 2:(5,6) 3:(7,8) 4:(9)

    if (need_clear) { need_clear = 0; LCD1602_clear(); }

    /* 1. 渲染当前手动选中的 LCD 菜单页面 */
    if (first_flag) {
        first_flag = 0;
        LCD1602_display(1, 1, "Select Function:");
        switch (page) {
            case 0: LCD1602_display(2, 1, "1:Bri   2:Temp "); break;
            case 1: LCD1602_display(2, 1, "3:Music 4:Calc  "); break;
            case 2: LCD1602_display(2, 1, "5:Time  6:UART  "); break;
            case 3: LCD1602_display(2, 1, "7:Piano 8:Count "); break;
            case 4: LCD1602_display(2, 1, "9:React         "); break;
            default: page = 0; break;
        }
    }

    /* 2. 检测独立按键 K3 单击 (向左/上循环切页) */
    if (key_check(KEY_3, KEY_SINGLE)) {
        if (page == 0) {
            page = 4; // 在最左侧界面左切时 -> 闭环切换到最右侧界面
        } else {
            page--;
        }
        first_flag = 1; // 触发 LCD 重新刷新渲染
    }

    /* 3. 检测独立按键 K4 单击 (向右/下循环切页) */
    if (key_check(KEY_4, KEY_SINGLE)) {
        if (page == 4) {
            page = 0; // 在最右侧界面右切时 -> 闭环切换到最左侧界面
        } else {
            page++;
        }
        first_flag = 1; // 触发 LCD 重新刷新渲染
    }

    /* 4. 根据矩阵键值 1~9 零延迟直接切入对应工具 */
    switch (matrix_key)
    {
        case MATRIX_S1: system_state = STATE_SHOW_BRIGHT; first_flag = 1; need_clear = 1; break;
        case MATRIX_S2: system_state = STATE_SHOW_TEMP;   first_flag = 1; need_clear = 1; break;
        case MATRIX_S3: system_state = STATE_MUSIC;       first_flag = 1; need_clear = 1; break;
        case MATRIX_S4: system_state = STATE_CALC;        first_flag = 1; need_clear = 1; break;
        case MATRIX_S5: system_state = STATE_SHOW_TIME;   first_flag = 1; need_clear = 1; break;
        case MATRIX_S6: system_state = STATE_UART;        first_flag = 1; need_clear = 1; break;
        case MATRIX_S7: system_state = STATE_PIANO;       first_flag = 1; need_clear = 1; break;
        case MATRIX_S8: system_state = STATE_COUNTDOWN;   first_flag = 1; need_clear = 1; break;
        case MATRIX_S9: system_state = STATE_REACT;       first_flag = 1; need_clear = 1; break;
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
    int current_temp;   // 温度定点值，单位 0.01℃（如 25.50℃ → 2550）
    if (first_flag) { first_flag = 0; LCD1602_clear(); LCD1602_display(1, 3, "Desk Temp:"); ds18B20_convertT(); }
    if (sysTick_checked(&temp_tick, 500)) {
        current_temp = ds18B20_read_temp_c100();
        LCD_DisplayTemp(2, 5, current_temp);
        ds18B20_convertT();
        /* 物理超温判断（40.00℃） */
        if (current_temp >= 4000) {
            TR1 = 0; ET1 = 0; // 强行拉住 Timer1 发生器，防止波形碎裂
            Beep = 0;         // 连续拉低电平，制造无间断长鸣警告
        } else {
            if (!Buzzer_IsPlaying()) Beep = 1; // 释放恢复正常高电平
        }
    }
}

/* 选项三：青花瓷非阻塞音乐播放（曲谱循环播放，无需“结束”提示） */
static void music_service(void){
    if (first_flag) {
        first_flag = 0;
        LCD1602_clear();
        LCD1602_display(1, 3, "Playing...");
        LCD1602_display(2, 2, "Qing Hua Ci");
        Buzzer_PlayQingHuaCi();
    }
    Buzzer_Service(); // 轮询无阻塞更新谱表节奏
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
                LCD1602_clear_line(2);        // 只清数字行，保留顶部提示
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
                    first_num = first_num * 10 + d;
                    dot_cnt++;
                }
                LCD1602_clear_line(2);
                LCD_DisplayLong(2, 1, first_num);
                if (has_dot) LCD1602_display_char(2, 15, '.'); // 修复：正确显示小数点
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
                LCD1602_clear_line(2);
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
                    second_num = second_num * 10 + d;
                    dot_cnt++;
                }
                LCD1602_clear_line(2);
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

/* 选项六：上位机串口接收服务（只刷新字符位，不再整行重绘） */
static void uart_service(void){
    static bit rx_line_shown = 0;
    u8 rx;
    if (first_flag) {
        first_flag = 0; LCD1602_clear();
        LCD1602_display(1, 2, "UART Receiver");
        LCD1602_display(2, 1, "Waiting Link...");
        UART_SetMode(1); // 开启并配置串口
        UART_SendString("UART Mode Ready!\r\n");
        rx_line_shown = 0;
    }
    rx = UART_Service(); // 轮询接收
    if (rx != 0xFF) {
        if (!rx_line_shown) {
            rx_line_shown = 1;
            LCD1602_display(2, 1, "Char Recv: [ ]  ");
        }
        LCD1602_display_char(2, 13, rx); // 实时将收到的可见 ASCII 字符渲染到屏幕上
    }
}

/* 选项七：电子琴（S1~S16 弹奏 16 个音阶，按下后发声 250ms 自动停止） */
static void piano_service(void){
    static u16 idata piano_tick = 0;
    if (first_flag) {
        first_flag = 0;
        LCD1602_clear();
        LCD1602_display(1, 1, "Piano S1~S16");
        LCD1602_display(2, 1, "Key:");
    }
    if (matrix_key >= 1 && matrix_key <= 16) {
        LCD_DisplayNum2(2, 5, matrix_key);      // 先刷新显示，再发声：
        Buzzer_PlayTone((u8)(10 + matrix_key)); // P2.5 与 LCD_RW 共用，避免响铃时写屏冲突
        piano_tick = 0;                         // 重新开始 250ms 自动停音计时
    }
    if (TR1 != 0 && sysTick_checked(&piano_tick, 250)) {
        Buzzer_Stop();                          // 自动停音，释放 P2.5
    }
}

/* 选项八：倒计时器（4 位数字设定 MM:SS，K4 启停、K3 清零，结束长鸣提示） */
static void countdown_service(void){
    static u32 idata cd_remain_ms = 0;  // 剩余毫秒数
    static u16 idata cd_raw = 0;        // 用户输入的 4 位 MMSS 原始值
    static u8   idata cd_digits = 0;    // 已输入位数（0~4）
    static u16  idata cd_tick = 0;      // 250ms 倒计时节拍
    static bit  cd_running = 0;         // 是否正在倒计时
    static bit  cd_done = 0;            // 是否已到点
    u16 sec;
    u8 d;

    if (first_flag) {
        first_flag = 0;
        Buzzer_Stop();                  // 进入界面先关掉可能残留的提示音
        cd_remain_ms = 0; cd_raw = 0; cd_digits = 0;
        cd_running = 0; cd_done = 0;
        LCD1602_clear();
        LCD1602_display(1, 1, "CountDown MM:SS");
        LCD1602_display(2, 1, "Set: ");
        LCD_DisplayNum4(2, 6, cd_raw);
    }

    /* 倒计时结束：持续长鸣提示，任意键退出 */
    if (cd_done) {
        if (matrix_key != 0 || key_check(KEY_4, KEY_SINGLE) || key_check(KEY_3, KEY_SINGLE)) {
            Buzzer_Stop();
            cd_done = 0;
            first_flag = 1;
        }
        return;
    }

    /* 暂停状态：K4 继续，K3 重置 */
    if (!cd_running && cd_remain_ms > 0) {
        if (key_check(KEY_4, KEY_SINGLE)) cd_running = 1;
        if (key_check(KEY_3, KEY_SINGLE)) { cd_remain_ms = 0; first_flag = 1; }
    }

    /* 设置状态：输入 4 位数字（前 2 位分钟，后 2 位秒） */
    if (!cd_running && cd_remain_ms == 0) {
        if (matrix_key != 0) {
            d = calc_get_digit(matrix_key);
            if (d <= 9 && cd_digits < 4) {
                cd_raw = (u16)(cd_raw * 10u + d);
                cd_digits++;
                LCD_DisplayNum4(2, 6, cd_raw);
            }
        }
        if (key_check(KEY_4, KEY_SINGLE) && cd_digits == 4) {
            /* MMSS → 总秒数 → 总毫秒 */
            u16 mm = (u16)(cd_raw / 100);
            u16 ss = (u16)(cd_raw % 100);
            if (ss > 59) ss = 59;   // 秒数上限 59，防止 99:99 越界显示
            cd_remain_ms = ((u32)mm * 60u + ss) * 1000u;
            cd_running = 1;
            LCD1602_clear_line(2);
            LCD1602_display(2, 1, "Run:");
            sec = (u16)((cd_remain_ms + 999u) / 1000u);
            LCD_DisplayNum2(2, 7, (u16)(sec / 60));
            LCD1602_display_char(2, 9, ':');
            LCD_DisplayNum2(2, 10, (u16)(sec % 60));
        }
        if (key_check(KEY_3, KEY_SINGLE)) {
            cd_raw = 0; cd_digits = 0;
            LCD1602_display(2, 1, "Set: ");
            LCD_DisplayNum4(2, 6, cd_raw);
        }
    }

    /* 运行状态：每 250ms 递减，到点触发长鸣 */
    if (cd_running) {
        if (key_check(KEY_4, KEY_SINGLE)) {
            cd_running = 0;             // K4 暂停
        } else if (key_check(KEY_3, KEY_SINGLE)) {
            cd_running = 0; cd_remain_ms = 0; first_flag = 1;  // K3 重置
        } else if (sysTick_checked(&cd_tick, 250)) {
            if (cd_remain_ms <= 250) {
                cd_remain_ms = 0;
                cd_running = 0;
                cd_done = 1;
                Buzzer_Stop();          // 先停音，保证下面写屏期间 P2.5 不翻转
                LCD1602_display(1, 1, "Time Up!   ");
                LCD1602_display(2, 1, "Any Key Exit");
                Buzzer_PlayTone(20);    // 再触发结束长鸣（约 880Hz）
            } else {
                cd_remain_ms -= 250;
                sec = (u16)((cd_remain_ms + 999u) / 1000u);
                LCD_DisplayNum2(2, 7, (u16)(sec / 60));
                LCD1602_display_char(2, 9, ':');
                LCD_DisplayNum2(2, 10, (u16)(sec % 60));
            }
        }
    }
}

/* 选项九：反应速度测试（随机延时后显示 GO，测按键反应毫秒数） */
static void react_service(void){
    static u8   idata r_phase = 0;     // 0=待开始 1=等待GO 2=已显示GO 3=显示成绩
    static u16  idata r_wait_ms = 0;   // 随机等待时长（500~2499ms）
    static u16  idata r_tick = 0;      // 等待计时
    static u16  idata r_start_ms = 0;  // GO 出现的系统滴答
    static u16  idata r_result = 0;    // 反应时间（ms）
    static u16  idata r_rng = 0x4D2;   // 简易伪随机数种子

    if (first_flag) {
        first_flag = 0;
        r_phase = 0;
        LCD1602_clear();
        LCD1602_display(1, 1, "React Test");
        LCD1602_display(2, 1, "K4=Start K3=No");
    }

    switch (r_phase)
    {
        case 0: // 待开始
            if (key_check(KEY_4, KEY_SINGLE)) {
                r_rng = (u16)(r_rng * 13u + 7u);          // 线性同余伪随机
                r_wait_ms = (u16)(500u + r_rng % 2000u);  // 0.5s~2.5s 随机等待
                r_tick = 0;
                r_phase = 1;
                LCD1602_clear_line(2);
                LCD1602_display(2, 1, "Wait GO...");
            }
            break;

        case 1: // 随机等待中
            if (sysTick_checked(&r_tick, r_wait_ms)) {
                r_phase = 2;
                r_start_ms = get_current_sysTick();
                LCD1602_clear_line(2);
                LCD1602_display(2, 6, "GO!!");
            }
            break;

        case 2: // 已显示 GO，等待按下任意键
            if (matrix_key != 0 || key_check(KEY_4, KEY_SINGLE) || key_check(KEY_3, KEY_SINGLE)) {
                r_result = (u16)(get_current_sysTick() - r_start_ms);
                r_phase = 3;
                LCD1602_clear_line(2);
                LCD1602_display(2, 1, "Time:");
                LCD_DisplayNum4(2, 7, r_result);
                LCD1602_display(2, 12, "ms");
            }
            break;

        case 3: // 成绩已绘制，等待 K4 重玩
            if (key_check(KEY_4, KEY_SINGLE)) {
                r_phase = 0;
                LCD1602_clear_line(2);
                LCD1602_display(2, 1, "K4=Start K3=No");
            }
            break;
    }
}

/* ==================== 矩阵键值基础转换映射工具（查表法，体积更小） ==================== */
/* key_digit[]: 键号(1~16) → 数字(0~9)，0xFF 表示该键不是数字键 */
static u8 code key_digit[17] = {
    0xFF,          // 0: 占位（无键0）
    1, 2, 3, 0xFF, // S1~S4
    4, 5, 6, 0xFF, // S5~S8
    7, 8, 9, 0xFF, // S9~S12
    0xFF, 0, 0xFF, 0xFF  // S13~S16
};

static u8 calc_get_digit(u8 key){
    return (u8)((key <= 16) ? key_digit[key] : 0xFF);
}

static bit calc_is_digit(u8 key){
    return (key <= 16) && (key_digit[key] != 0xFF);
}
