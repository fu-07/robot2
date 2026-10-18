/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "OLED.h"
#include "bluetooth.h"
#include "stdio.h"
#include "key.h"
#include "motion.h"

/* USER CODE END Includes */
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* 比赛参数定义 */
#define TARGET_STEPS 60        // 走完900mm需要的步态循环次数（先写60，实测后调整）
#define STEP_DELAY 500         // 单步步态之间的延时（毫秒）

/* 全局变量 */
// 舵机相关变量
volatile MotionState currentMotion = MOTION_STOP;
volatile uint16_t motionSteps = 0;


uint32_t gait_tick = 0;  //步态时间戳，非阻塞
/* USER CODE END PV */
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 角度转PWM脉宽计数值（0.5ms~2.5ms对应500~2500）
uint16_t AngleToPulse(float angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return (uint16_t)(angle / 180.0f * 2000.0f + 500.0f);
}
// 设置舵机角度（按你引脚分配）
void SetServo(uint8_t id, float angle) {
    uint16_t pulse = AngleToPulse(angle);
    switch(id) {
        case 1: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse); break; // PA0
        case 2: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse); break; // PA1
        case 3: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse); break; // PA2
        case 4: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse); break; // PA3
        case 5: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse); break; // PA6
        case 6: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse); break; // PA7
        case 7: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse); break; // PB0
        case 8: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pulse); break; // PB1
    }
}


// 站立姿态
void Robot_Stand(void) {
    for(int i = 1; i <= 8; i++) SetServo(i, 90.0f);
}


// 前进单步（对角步态，暂时不调用）
void Robot_Walk_Forward_Step(void) {
    static uint8_t stepPhase = 0;
    switch(stepPhase) {
        case 0: // 抬起左前和右后腿
            SetServo(2, 70.0f); SetServo(8, 110.0f);
            stepPhase = 1; break;
        case 1: // 左前、右后向前摆动
            SetServo(1, 120.0f - 25); SetServo(7, 120.0f + 25);
            stepPhase = 2; break;
        case 2: // 放下左前和右后腿
            SetServo(2, 90.0f); SetServo(8, 90.0f);
            stepPhase = 3; break;
        case 3: // 抬起右前和左后腿
            SetServo(4, 110.0f); SetServo(6, 70.0f);
            stepPhase = 4; break;
        case 4: // 右前、左后向前摆动
            SetServo(3, 60.0f + 25); SetServo(5, 60.0f - 25);
            stepPhase = 5; break;
        case 5: // 放下右前和左后腿
            SetServo(4, 90.0f); SetServo(6, 90.0f);
            stepPhase = 0; break;
    }
}

//======== 封装函数：蓝牙指令处理 ========
void BT_Process(void)
{
    if(bt_recv_flag == 1)
    {
        bt_recv_flag = 0;
        BT_SendString("Echo:");
        BT_SendByte(bt_recv_char);
        BT_SendString("\r\n");
        if(bt_recv_char == '1')
        {
            BT_SendString("Start Walk\r\n");
            currentMotion = MOTION_FORWARD;
            motionSteps = 0;
            gait_tick = HAL_GetTick();
        }
        else if(bt_recv_char == '0')
        {
            BT_SendString("Stop\r\n");
            currentMotion = MOTION_STOP;
            Robot_Stand();
        }
    }
}


//======== 封装函数：步态状态机调度（非阻塞，主循环持续调用） ========
void Robot_Gait_Run(void)
{
    char oled_buf[20];
    if(currentMotion == MOTION_FORWARD)
    {
        if(HAL_GetTick() - gait_tick > STEP_DELAY)
        {
            gait_tick = HAL_GetTick();
            Robot_Walk_Forward_Step();
            motionSteps ++;
            //走到设定步数，自动停下
            if(motionSteps >= TARGET_STEPS)
            {
                currentMotion = MOTION_STOP;
                Robot_Stand();
                BT_SendString("Reach Target, Stop\r\n");
            }
        }
        //OLED刷新：行走状态 + 当前步数
        OLED_Clear();
        sprintf(oled_buf,"State: WALK");
        OLED_ShowString(0,0,oled_buf,12);
        sprintf(oled_buf,"Step:%d/%d",motionSteps, TARGET_STEPS);
        OLED_ShowString(0,16,oled_buf,12);
        OLED_Update();
    }
    else
    {
        // MOTION_STOP 显示停止
        OLED_Clear();
        sprintf(oled_buf,"State: STOP");
        OLED_ShowString(0,0,oled_buf,12);
        sprintf(oled_buf,"Step:%d/%d",motionSteps, TARGET_STEPS);
        OLED_ShowString(0,16,oled_buf,12);
        OLED_Update();
    }
}

/* USER CODE END 0 */
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
// 启动TIM2的4路PWM（舵机1~4）
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
// 启动TIM3的4路PWM（舵机5~8）
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

//蓝牙模块初始化
  BT_Init();

// OLED显示初始化
HAL_Delay(500); // 加上这一句！等屏幕完全上电
  OLED_Init();
  OLED_Clear();
  OLED_ShowFace();
  
  OLED_Update(); // 把缓存推给屏幕
  //按键模块初始化
  KEY_Init();    //新增按键模块初始化
  
  Robot_Stand();
  /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


  

  
      HAL_Delay(100);
  




    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    // 测试模式：主循环空跑，**不执行任何步态**
  }
  /* USER CODE END 3 */
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_ClkInitStruct.APB1CLKDivider|RCC_ClkInitStruct.APB2CLKDivider;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}
/* USER CODE BEGIN 4 */

/* USER CODE END 4 */
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error line source number has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
