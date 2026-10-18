#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f1xx_hal.h"
// 蓝牙接收和标志位
extern uint8_t bt_recv_char;
extern uint8_t bt_recv_flag;

void BT_Init(void);
void BT_SendString(char *str);
void BT_SendByte(uint8_t dat);

#endif

