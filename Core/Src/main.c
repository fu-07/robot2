/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
/* USER CODE BEGIN Includes */
#include "string.h"
#include "OLED.h"
#include "bluetooth.h"
#include "stdio.h"
#include "key.h"
#include "motion.h"

/* Private define ------------------------------------------------------------*/
#define TARGET_STEPS 60       // 走完900mm需要的步态循环次数（实测后修改）
#define STEP_DELAY 100         // 单步步态之间的延时（毫秒）
#define SIDE_ANGLE 20           // 侧移/前进角度
#define TURN_ANGLE 25           // 转向角度

/* 全局变量 */
volatile MotionState currentMotion = MOTION_STOP;
volatile uint16_t motionSteps = 0;
uint32_t gait_tick = 0;
volatile uint8_t micro_step_request = 0; // 蓝牙微调标志

void SystemClock_Config(void);

// 角度转PWM脉宽计数值
uint16_t AngleToPulse(float angle) {
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    return (uint16_t)(angle / 180.0f * 2000.0f + 500.0f + 0.5f);
}

void SetServo(uint8_t id, float angle) {
    uint16_t pulse = AngleToPulse(angle);
    switch(id) {
        case 1: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse); break;
        case 2: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse); break;
        case 3: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse); break;
        case 4: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse); break;
        case 5: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse); break;
        case 6: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse); break;
        case 7: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse); break;
        case 8: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pulse); break;
    }
}

void Robot_Stand(void) {
    SetServo(1, 90.0f); SetServo(3, 90.0f); SetServo(5, 90.0f); SetServo(7, 90.0f);
    SetServo(2, 76.0f); SetServo(8, 100.0f); SetServo(4, 95.0f); SetServo(6, 80.0f);
}

void Robot_Prepare_Sideways(void)
{
    // 保持你原有的参数
    SetServo(1, 150.0f); 
    SetServo(3, 30.0f);  
    SetServo(5, 90.0f);  
    SetServo(7, 90.0f);  
    
    SetServo(2, 90.0f);
    SetServo(4, 90.0f);
    SetServo(6, 80.0f);
    SetServo(8, 100.0f);
}

// 前进单步（原版 8 相位）
void Robot_Walk_Forward_Step(void) {
    static uint8_t stepPhase = 0;
    
    switch(stepPhase) {
        case 0:
            Robot_Prepare_Sideways();
            stepPhase = 1;
            break;
        case 1:
            SetServo(2, 80.0f);
            SetServo(8, 110.0f);
            stepPhase = 2;
            break;
        case 2:
            SetServo(1, 120.0f - SIDE_ANGLE);
            SetServo(7, 120.0f + SIDE_ANGLE);
            stepPhase = 3;
            break;
        case 3:
            SetServo(2, 100.0f);
            SetServo(8, 90.0f);
            stepPhase = 4;
            break;
        case 4:
            SetServo(4, 110.0f);
            SetServo(6, 70.0f);
            stepPhase = 5;
            break;
        case 5:
            SetServo(3, 70.0f + SIDE_ANGLE);
            SetServo(5, 60.0f - SIDE_ANGLE);
            stepPhase = 6;
            break;
        case 6:
            SetServo(4, 90.0f);
            SetServo(6, 90.0f);
            stepPhase = 7;
            break;
        case 7:
            SetServo(1, 120.0f - SIDE_ANGLE/2);
            SetServo(3, 60.0f + SIDE_ANGLE/2);
            SetServo(5, 60.0f - SIDE_ANGLE/2);
            SetServo(7, 120.0f + SIDE_ANGLE/2);
            stepPhase = 0;
            break;
    }
}

// 左转单步（8相位错峰版）
void Robot_Turn_Left_Step(void) {
    static uint8_t stepPhase = 0;
    
    switch(stepPhase) {
        case 0: Robot_Stand(); stepPhase = 1; break;
        case 1: 
            SetServo(2, 70.0f); SetServo(8, 110.0f); 
            stepPhase = 2; break;
        case 2: 
            SetServo(1, 90.0f + TURN_ANGLE); SetServo(7, 90.0f + TURN_ANGLE); 
            stepPhase = 3; break;
        case 3: 
            SetServo(2, 90.0f); SetServo(8, 90.0f); 
            stepPhase = 4; break;
        case 4: 
            SetServo(4, 110.0f); SetServo(6, 70.0f); 
            stepPhase = 5; break;
        case 5: 
            SetServo(3, 90.0f + TURN_ANGLE); SetServo(5, 90.0f + TURN_ANGLE); 
            stepPhase = 6; break;
        case 6: 
            SetServo(4, 90.0f); SetServo(6, 90.0f); 
            stepPhase = 7; break;
        case 7: 
            Robot_Stand(); stepPhase = 0; break;
    }
}

// 右转单步（8相位错峰版）
void Robot_Turn_Right_Step(void) {
    static uint8_t stepPhase = 0;
    
    switch(stepPhase) {
        case 0: Robot_Stand(); stepPhase = 1; break;
        case 1: 
            SetServo(2, 75.0f); SetServo(8, 110.0f); 
            stepPhase = 2; break;
        case 2: 
            SetServo(1, 80.0f - TURN_ANGLE); SetServo(7, 90.0f - TURN_ANGLE); 
            stepPhase = 3; break;
        case 3: 
            SetServo(2, 90.0f); SetServo(8, 90.0f); 
            stepPhase = 4; break;
        case 4: 
            SetServo(4, 110.0f); SetServo(6, 70.0f); 
            stepPhase = 5; break;
        case 5: 
            SetServo(3, 80.0f - TURN_ANGLE); SetServo(5, 90.0f - TURN_ANGLE); 
            stepPhase = 6; break;
        case 6: 
            SetServo(4, 90.0f); SetServo(6, 90.0f); 
            stepPhase = 7; break;
        case 7: 
            Robot_Stand(); stepPhase = 0; break;
    }
}

// 蓝牙指令处理
void BT_Process(void) {
    if(bt_recv_flag == 1) {
        bt_recv_flag = 0;
        if(bt_recv_char == '1') { // 启动直行
            currentMotion = MOTION_FORWARD;
            motionSteps = 0;
            gait_tick = HAL_GetTick();
        }
        else if(bt_recv_char == '0') { // 急停
            currentMotion = MOTION_STOP;
            Robot_Stand();
        }
        else if(bt_recv_char == '2') { // 单步微调
            micro_step_request = 1;
        }
        else if(bt_recv_char == '3') { // 左转
            currentMotion = MOTION_TURN_LEFT;
            gait_tick = HAL_GetTick();
        }
        else if(bt_recv_char == '4') { // 右转
            currentMotion = MOTION_TURN_RIGHT;
            gait_tick = HAL_GetTick();
        }
    }
}

void Robot_Gait_Run(void) {
    
    // 1. 处理微调指令（发'2'）
    if(micro_step_request && currentMotion == MOTION_STOP) {
        micro_step_request = 0;
        for(int i = 0; i < 8; i++) {
            Robot_Walk_Forward_Step();
            HAL_Delay(STEP_DELAY); 
        }
        motionSteps++; 
    }

    // 2. 处理自动直行
    if(currentMotion == MOTION_FORWARD) {
        if(HAL_GetTick() - gait_tick > STEP_DELAY) {
            gait_tick = HAL_GetTick();
            Robot_Walk_Forward_Step(); 
            
            static uint8_t phase_count = 0;
            phase_count++;
            
            if(phase_count >= 8) {
                phase_count = 0;
                motionSteps ++; 
            }
            
            if(motionSteps >= TARGET_STEPS) {
                currentMotion = MOTION_STOP;
                Robot_Stand();
            }
        }
    }
    
    // 3. 处理原地左转
    if(currentMotion == MOTION_TURN_LEFT) {
        if(HAL_GetTick() - gait_tick > STEP_DELAY) {
            gait_tick = HAL_GetTick();
            Robot_Turn_Left_Step();
        }
    }
    
    // 4. 处理原地右转
    if(currentMotion == MOTION_TURN_RIGHT) {
        if(HAL_GetTick() - gait_tick > STEP_DELAY) {
            gait_tick = HAL_GetTick();
            Robot_Turn_Right_Step();
        }
    }
}

/* 画实心圆，供眼睛使用 */
static void OLED_DrawFilledCircle(int cx, int cy, int r, u8 color)
{
    int dx, dy, x, y;

    for(dy = -r; dy <= r; dy++)
    {
        for(dx = -r; dx <= r; dx++)
        {
            if(dx * dx + dy * dy <= r * r)
            {
                x = cx + dx;
                y = cy + dy;

                if(x >= 0 && x < 128 && y >= 0 && y < 64)
                {
                    OLED_DrawPoint((u8)x, (u8)y, color);
                }
            }
        }
    }
}

void OLED_ShowFace(u8 cx, u8 cy, u8 mood)
{
    int i, dx, y;
    int left_eye_x, right_eye_x, eye_y, mouth_y;

    if(cx < 28 || cx > 99 || cy < 20 || cy > 44)
    {
        cx = 64;
        cy = 32;
    }

    OLED_Clear();

    left_eye_x  = (int)cx - 14;
    right_eye_x = (int)cx + 14;
    eye_y       = (int)cy - 8;

    if(mood == 1)  
    {
        OLED_DrawFilledCircle(left_eye_x,  eye_y, 3, 1);
        OLED_DrawFilledCircle(right_eye_x, eye_y, 3, 1);
    }
    else if(mood == 3)  
    {
        OLED_DrawFilledCircle(left_eye_x,  eye_y, 5, 1);
        OLED_DrawFilledCircle(right_eye_x, eye_y, 5, 1);
    }
    else
    {
        OLED_DrawFilledCircle(left_eye_x,  eye_y, 4, 1);
        OLED_DrawFilledCircle(right_eye_x, eye_y, 4, 1);
    }

    if(mood == 0)  
    {
        mouth_y = (int)cy + 14;
        for(i = -14; i <= 14; i++)
        {
            dx = i;
            y = mouth_y - (dx * dx) / 22;
            if(y >= 0 && y < 64)
            {
                OLED_DrawPoint((u8)((int)cx + dx), (u8)y, 1);
                if(y + 1 < 64) OLED_DrawPoint((u8)((int)cx + dx), (u8)(y + 1), 1);
            }
        }
    }
    else if(mood == 1)  
    {
        mouth_y = (int)cy + 6;
        for(i = -14; i <= 14; i++)
        {
            dx = i;
            y = mouth_y + (dx * dx) / 22;
            if(y >= 0 && y < 64)
            {
                OLED_DrawPoint((u8)((int)cx + dx), (u8)y, 1);
                if(y + 1 < 64) OLED_DrawPoint((u8)((int)cx + dx), (u8)(y + 1), 1);
            }
        }
    }
    else if(mood == 3)  
    {
        OLED_DrawCircle(cx, (u8)((int)cy + 14), 5);
    }
    else  
    {
        OLED_DrawLine((u8)((int)cx - 12), (u8)((int)cy + 14), (u8)((int)cx + 12), (u8)((int)cy + 14), 1);
    }

    OLED_Refresh();
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();

  // 启动8路PWM
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

  // 初始化模块
  BT_Init();
  
  HAL_Delay(500); 
  OLED_Init();
  OLED_Clear();

  OLED_ShowFace(64, 32, 0);   // 显示微笑脸
  
  KEY_Init();
  Robot_Stand();

  while (1)
  {
      BT_Process();      // 读取蓝牙
      Robot_Gait_Run();  // 执行步态
      
      HAL_Delay(10);     // 防止主循环跑太快
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
