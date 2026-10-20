/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "OLED.h"
#include "bluetooth.h"
#include "stdio.h"
#include "key.h"
#include "motion.h"

/* Private define ------------------------------------------------------------*/
#define TARGET_STEPS 95        // 走完900mm需要的步态循环次数（实测后修改）
#define STEP_DELAY 50         // 单步步态之间的延时（毫秒）

/* 全局变量 */
volatile MotionState currentMotion = MOTION_STOP;
volatile uint16_t motionSteps = 0;
uint32_t gait_tick = 0;
volatile uint8_t micro_step_request = 0; // 蓝牙微调标志

void SystemClock_Config(void);

// 角度转PWM脉宽计数值
uint16_t AngleToPulse(float angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return (uint16_t)(angle / 180.0f * 2000.0f + 500.0f);
}

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

void Robot_Stand(void) {
    // 髋关节保持正中，指向前后
    SetServo(1, 90.0f); 
    SetServo(3, 90.0f); 
    SetServo(5, 90.0f); 
    SetServo(7, 90.0f);
    
    // 膝关节稍微弯曲（注意左右镜像，数值可能相反，比如左边75右边105）
    SetServo(2, 75.0f); SetServo(8, 105.0f); 
    SetServo(4, 94.f); SetServo(6, 78.0f); 
}


void Robot_Walk_Forward_Step(void) {
    static uint8_t stepPhase = 0;
    
    switch(stepPhase) {
        case 0: 
            // 第一步：左前(1)和右后(7)往前划，右前(3)和左后(5)往后推。
            // 绝对对称，绝不产生侧翻扭矩！
            SetServo(1, 60.0f);  // 左前
            SetServo(7, 120.0f); // 右后（镜像安装，所以角度是反向的）
            SetServo(3, 120.0f); // 右前
            SetServo(5, 60.0f);  // 左后
            HAL_Delay(150);      // 给摩擦力发挥作用的时间
            stepPhase = 1; 
            break;
            
        case 1: 
            // 第二步：交换方向，右前(3)和左后(5)往前，左前(1)和右后(7)往后。
            SetServo(3, 60.0f);  // 右前
            SetServo(5, 120.0f); // 左后
            SetServo(1, 120.0f); // 左前
            SetServo(7, 60.0f);  // 右后
            HAL_Delay(150);      
            stepPhase = 0; 
            break;
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
    }
}

// 步态状态机调度 + OLED显示
void Robot_Gait_Run(void) {
    char oled_buf[20];
    static uint32_t oled_tick = 0;

    // 1. 处理微调指令
    if(micro_step_request && currentMotion == MOTION_STOP) {
        micro_step_request = 0;
        Robot_Walk_Forward_Step();
        motionSteps++;
    }

    // 2. 处理自动直行
    if(currentMotion == MOTION_FORWARD) {
        if(HAL_GetTick() - gait_tick > STEP_DELAY) {
            gait_tick = HAL_GetTick();
            Robot_Walk_Forward_Step();
            motionSteps ++;
            
            if(motionSteps >= TARGET_STEPS) {
                currentMotion = MOTION_STOP;
                Robot_Stand();
            }
        }
    }
    
    // 3. 每200ms刷新一次屏幕（防止刷屏太快拖慢系统）
    if(HAL_GetTick() - oled_tick > 200) {
        oled_tick = HAL_GetTick();
        OLED_Clear();
        if(currentMotion == MOTION_FORWARD) {
            sprintf(oled_buf,"State: WALK");
            OLED_ShowString(0, 0, (u8*)oled_buf, 16, 1); // 加上了 (u8*) 和 最后一个参数 1
            sprintf(oled_buf,"Step:%d/%d",motionSteps, TARGET_STEPS);
            OLED_ShowString(0, 16, (u8*)oled_buf, 16, 1); // 加上了 (u8*) 和 最后一个参数 1
        } else {
            sprintf(oled_buf,"State: STOP");
            OLED_ShowString(0, 0, (u8*)oled_buf, 16, 1);
            sprintf(oled_buf,"Step:%d/%d",motionSteps, TARGET_STEPS);
            OLED_ShowString(0, 16, (u8*)oled_buf, 16, 1);
        }
        OLED_Refresh(); // 把这个库的 Update 改成 Refresh
    }
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
  
  KEY_Init();
  Robot_Stand();

  while (1)
  {
      BT_Process();      // 读取蓝牙
      // KEY_Process();  // 如果你用按键就加上
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
