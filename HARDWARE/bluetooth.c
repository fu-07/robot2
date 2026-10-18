#include "bluetooth.h"
#include <string.h>
#include "usart.h"

static uint8_t bt_rx_byte;

// 全局变量
uint8_t bt_recv_char;
uint8_t bt_recv_flag = 0;

// 蓝牙接收回调
static void User_BT_Rx(uint8_t ch)
{
    bt_recv_char = ch;
    bt_recv_flag = 1; // 只打标记
}

void BT_Init(void)
{
    HAL_UART_Receive_IT(&huart1, &bt_rx_byte, 1);
}

// HAL串口中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        User_BT_Rx(bt_rx_byte); // 直接调用，删掉函数指针那一堆
        HAL_UART_Receive_IT(&huart1, &bt_rx_byte, 1);
    }
}

// 发送函数不变
void BT_SendString(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
}
void BT_SendByte(uint8_t dat)
{
    HAL_UART_Transmit(&huart1, &dat, 1, 100);
}
