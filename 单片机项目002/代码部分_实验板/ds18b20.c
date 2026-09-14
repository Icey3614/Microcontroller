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

float ds18B20_read_temperture(void)
{
    u8 TLSB, TMSB; // [cite: 1155]
    int temp; // [cite: 1156]
    float temperture = 0.0f; // [cite: 1157]

    AD_CS_Isolation = 1; // 强制隔离保护触控芯片 [cite: 1158]

    if (oneWire_init() == 0) // 有器件应答 [cite: 1159]
    {
        oneWire_sendByte(SKIP_ROM); // [cite: 1161]
        oneWire_sendByte(READ_REGISTER); // [cite: 1162]
        TLSB = oneWire_receiveByte(); // [cite: 1163]
        TMSB = oneWire_receiveByte(); // [cite: 1164]
        temp = ((int)TMSB << 8) | TLSB; // [cite: 1165]

        // 处理正负温度 [cite: 1166]
        if (TMSB & 0xF8) // [cite: 1167]
        {
            temp = ~temp + 1; // [cite: 1169]
            temperture = -temp / 16.0f; // [cite: 1170]
        }
        else // [cite: 1172]
        {
            temperture = temp / 16.0f; // [cite: 1174]
        }
    }
    return temperture; // [cite: 1177]
}