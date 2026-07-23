#include "key.h"  //独立按键

#define KEY_PRESSED    1  //表示按下
#define KEY_UNPRESSED  0  //表示松开

#define KEY_TIME_LONG    1000  // 刚好对应你要求的1秒判定，长按时间判定
#define KEY_TIME_REPEAT  200   //连按时的响应间隔

u8 idata key_flag[KEY_COUNT];  // idata 告诉编译器，
// 把这个数组存放在单片机内部的间接寻址 RAM 区（共 256 字节）。
// KEY_COUNT在key.h中被定义为4，在 idata中存储四个独立按键
u8 key_check(u8 key_n, u8 flag)  //按键检查函数
{
    if (key_flag[key_n] & flag) {
        if (flag != KEY_HOLD) key_flag[key_n] &= ~flag; 
			//&= ~flag（位清零运算），把对应的那个二进制位强行抹成 0
        return 1;
    }
    return 0;
}
/*
检测某个独立按键（key_n）是否是某个状态（flag）

其中用条件运算判断，是的化返回1，表示当前要查询的这个按键确实是推测的状态，
同时将其清零，避免对之后造成干扰

不符合的话返回0，表明不是推测的状态
*/
static u8 key_getState(u8 KEY_n)//静态限定符，防止信息丢失
{
    if (KEY_n == KEY_1) return (key_1_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
	/*去检测key_1_pin（P3.0引脚）的电平是不是 0 
		如果是的（等于0），就返回 KEY_PRESSED（按键压下标签） 
		如果不是（等于1），就返回 KEY_UNPRESSED（按键松开标签） 
	*/
    if (KEY_n == KEY_2) return (key_2_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    if (KEY_n == KEY_3) return (key_3_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    if (KEY_n == KEY_4) return (key_4_pin == 0) ? KEY_PRESSED : KEY_UNPRESSED;
    return KEY_UNPRESSED;
	/*
	最后这一行起到兜底的作用，默默地返回一个 KEY_UNPRESSED（按键松开）
	从而确保单片机不会因为读到未知状态而引发系统崩溃或误触发开关机。
	*/
}

void key_tick(void)
{
    static u8 idata currState[KEY_COUNT] = {KEY_UNPRESSED};  
		//定义当前四个独立按键的状态
    static u8 idata preState[KEY_COUNT] = {KEY_UNPRESSED};
		//定义上一时刻四个独立按的状态
    static u8 idata scan_div = 0;
		//10ms消抖计时器
    static u8 idata state[KEY_COUNT] = {0};
		//记录 4 个按键各自正处于 状态机的第几阶段
    static u16 idata time_dec[KEY_COUNT] = {0};
		//4 个按键各自专属的 毫秒级减法倒计时器
    u8 i;
		//普通的循环计数变量（没有 static，不需要记忆）
    for (i = 0; i < KEY_COUNT; i++) {
        if (time_dec[i] > 0) time_dec[i]--;
    }//四个独立按键

    scan_div++;
    if (scan_div >= 10) // 10ms物理消抖周期
    {
        scan_div = 0;
        for (i = 0; i < KEY_COUNT; i++)
        {
            preState[i] = currState[i];
            currState[i] = key_getState(i);

            if (currState[i] == KEY_PRESSED) key_flag[i] |= KEY_HOLD;
            else key_flag[i] &= ~KEY_HOLD;
					/*
					如果发现当前（currState）这一时刻按键是被压下的状态 ，
					就在它的（key_flag）上标记 KEY_HOLD（按住）
					否则（说明现在按键是松开的状态） ，
					就必须立刻去把 KEY_HOLD（按住）的标记给抹除掉 。
					*/

            if (currState[i] == KEY_PRESSED && preState[i] == KEY_UNPRESSED) key_flag[i] |= KEY_DOWN;
            if (currState[i] == KEY_UNPRESSED && preState[i] == KEY_PRESSED) key_flag[i] |= KEY_UP;
								//判断两个10ms内按键的状态是否发生改变
            switch (state[i])
            {//最终确定每个独立按键的状态
                case 0:
                    if (currState[i] == KEY_PRESSED) { time_dec[i] = KEY_TIME_LONG; state[i] = 1; }
                    break;
                case 1: // 等待长按时间判定
                    if (currState[i] == KEY_UNPRESSED) { key_flag[i] |= KEY_SINGLE; state[i] = 0; }
                    else if (time_dec[i] == 0) { key_flag[i] |= KEY_LONG; time_dec[i] = KEY_TIME_REPEAT; state[i] = 2; }
                    break;
                case 2: // 长按连发状态
                    if (currState[i] == KEY_UNPRESSED) { state[i] = 0; }
                    else if (time_dec[i] == 0) { key_flag[i] |= KEY_REPEAT; time_dec[i] = KEY_TIME_REPEAT; }
                    break;
            }
        }
    }
}