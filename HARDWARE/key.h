#ifndef __KEY_H
#define __KEY_H

#include "main.h"

// 对外接口
void KEY_Init(void);
void KEY_Process(void);

// 按键事件标志（供外部中断回调）
extern volatile uint8_t key1_pressed;
extern volatile uint8_t key2_pressed;

#endif
