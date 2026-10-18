#include "key.h"
#include "gpio.h"
#include "motion.h"

// 对外接口
extern volatile MotionState currentMotion;
extern volatile uint16_t motionSteps;
extern uint32_t gait_tick;
extern void Robot_Stand(void);
extern void BT_SendString(uint8_t *str);

// 模块内部私有变量
static uint32_t key1_tick = 0;
static uint32_t key2_tick = 0;
volatile uint8_t key1_pressed = 0;
volatile uint8_t key2_pressed = 0;

/**
 * @brief 按键初始化，这里只预留，GPIO/EXTI由CubeMX自动生成MX_GPIO_Init()
 */
void KEY_Init(void)
{
    key1_pressed = 0;
    key2_pressed = 0;
    key1_tick = 0;
    key2_tick = 0;
}

/**
 * @brief 按键主循环处理（消抖+事件处理）
 */
void KEY_Process(void)
{
    if(key1_pressed == 1)
    {
        if(HAL_GetTick() - key1_tick > 200)
        {
            key1_tick = HAL_GetTick();
            BT_SendString("KEY1 pressed, STOP\r\n");
            currentMotion = MOTION_STOP;
            Robot_Stand();
        }
        key1_pressed = 0;
    }
    if(key2_pressed == 1)
    {
        if(HAL_GetTick() - key2_tick > 200)
        {
            key2_tick = HAL_GetTick();
            BT_SendString("KEY2 pressed, START\r\n");
            currentMotion = MOTION_FORWARD;
            motionSteps = 0;
            gait_tick = HAL_GetTick();
        }
        key2_pressed = 0;
    }
}

/**
 * @brief 外部中断回调，放到key.c
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY1_Pin) {
        key1_pressed = 1;
    }
    else if (GPIO_Pin == KEY2_Pin) {
        key2_pressed = 1;
    }
}
