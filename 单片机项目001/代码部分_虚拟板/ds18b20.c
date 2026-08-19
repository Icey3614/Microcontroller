#include "ds18b20.h"

// 显式声明外部触控芯片片选，用于拉高隔离 [cite: 1141, 1142]
sbit AD_CS_Isolation = P3^5;

u8 ds18B20_convertT(void)
{
    // 隔离保护：强制释放XPT2046对P3.7(DOUT)总线的可能占用 [cite: 1145]
    AD_CS_Isolation = 1; // [cite: 1146]

    if (oneWire_init() != 0) // [cite: 1147]
        return 0; // 不在线 [cite: 1148]
    oneWire_sendByte(SKIP_ROM); // [cite: 1149]
    oneWire_sendByte(CONVERT_T); // [cite: 1150]
    return 1;  // 在线 [cite: 1151]
}

int ds18B20_read_temp_c100(void)
{
    u8 TLSB, TMSB; // [cite: 1155]
    int temp; // [cite: 1156]

    AD_CS_Isolation = 1; // 强制隔离保护触控芯片 [cite: 1158]

    if (oneWire_init() == 0) // 有器件应答 [cite: 1159]
    {
        oneWire_sendByte(SKIP_ROM); // [cite: 1161]
        oneWire_sendByte(READ_REGISTER); // [cite: 1162]
        TLSB = oneWire_receiveByte(); // [cite: 1163]
        TMSB = oneWire_receiveByte(); // [cite: 1164]
        temp = ((int)TMSB << 8) | TLSB; // [cite: 1165]

        /* V2.0 定点化：DS18B20 原始值为 12 位补码，单位 1/16 ℃。
           温度×100 = 原始值 × 100 / 16 = 原始值 × 25 / 4，
           全程整数运算，彻底摆脱 Keil C51 浮点库，显著节省 Flash。 */
        return (int)(((long)temp * 25L) / 4L); // 单位 0.01℃（截断取整）
    }
    return 0; // 传感器不在线，返回 0.00℃
}
