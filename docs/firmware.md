# 固件说明

Keil MDK5 工程，STM32F103C8T6，HAL 库，由 STM32CubeMX 生成骨架 + 手写驱动。

## 1. 目录

```
robot2/
├── Core/
│   ├── Inc/       main.h  tim.h  usart.h  gpio.h  stm32f1xx_it.h
│   └── Src/       main.c  tim.c  usart.c  gpio.c  stm32f1xx_it.c  system_stm32f1xx.c
├── HARDWARE/      手写驱动
│   ├── OLED.c / OLED.h / OLED_Data.c / oledfont.h / bmp.h
│   ├── bluetooth.c / bluetooth.h
│   ├── key.c / key.h
│   └── motion.h（运动状态枚举）、motion.c（空）
├── Drivers/       ST HAL + CMSIS（勿改）
├── MDK-ARM/       robot.uvprojx  ← 用 Keil 打开
└── robot.ioc      CubeMX 工程
```

## 2. 编译

1. 安装 **Keil MDK5** 与 **ARM Compiler V5.06**（工程用的是 AC5，AC6 会报一堆 `__weak` / 内联汇编错误）。
2. 安装 `Keil.STM32F1xx_DFP` 器件包。
3. 打开 `MDK-ARM/robot.uvprojx` → `F7`。
4. 需要 hex 时在 `Options → Output` 勾选 **Create HEX File**。

> `HARDWARE/` 必须已在 Include Paths 中（`Options → C/C++ → Include Paths`）。

## 3. CubeMX 重新生成

`robot.ioc` 可以用 STM32CubeMX 打开并 **GENERATE CODE**，但注意：

- 只有 `USER CODE BEGIN/END` 之间的内容会被保留；
- 本工程的步态逻辑（`SetServo` / `Robot_Stand` / `Robot_Walk_Forward_Step` / `Robot_Gait_Run`）是手写并覆盖进 `main.c` 的，**不在 USER CODE 区**，重生成会丢失 —— 先备份 `Core/Src/main.c`；
- 重生成后核对：TIM2/TIM3 `PSC=71 / ARR=19999`；USART1 9600 且开全局中断；OLED 两脚 **Open-Drain + Pull-up**。

## 4. 烧录

| 方式 | 说明 |
| --- | --- |
| ST-Link | `Options → Debug → ST-Link Debugger`，`F8` 下载 |
| 串口 ISP | FlyMcu 烧 `robot.hex`；BOOT0=1 上电 → 烧录 → BOOT0=0 复位 |

## 5. 代码地图（`Core/Src/main.c`）

| 函数 | 作用 |
| --- | --- |
| `AngleToPulse()` | 角度 → CCR（500~2500，对应 0.5~2.5 ms） |
| `SetServo(id, angle)` | 舵机编号 → TIM 通道映射 |
| `Robot_Stand()` | 黄金站姿（四髋 90°，膝 76/95/80/100） |
| `Robot_Prepare_Sideways()` | 起步 / 结束准备姿态 |
| `Robot_Walk_Forward_Step()` | 8 相位前进（`static stepPhase`） |
| `Robot_Turn_Left_Step()` / `Right` | 8 相位原地转向 |
| `BT_Process()` | 蓝牙指令解析 |
| `Robot_Gait_Run()` | 非阻塞调度 + 微调处理 |
| `OLED_ShowFace()` | 开机表情 |

## 6. 待修复（最小改动）

```c
/* HARDWARE/bluetooth.c —— 中断与主循环共享，必须 volatile */
volatile uint8_t bt_recv_char;
volatile uint8_t bt_recv_flag = 0;
```

```c
/* Core/Src/main.c —— 转向加个步数上限，避免停不下来 */
if (currentMotion == MOTION_TURN_LEFT && ++turn_steps > 20) {
    currentMotion = MOTION_STOP;
    Robot_Stand();
}
```
