# 8 自由度四足仿生机器人（陆地仿生机器人挑战赛）

> STM32F103C8T6 + 8×SG90 + HC-05 + 自制 50×80 mm PCB + 3D 打印机身
> 任务：90 秒内沿 200 mm 宽、900 mm 长的赛道直行通过，并尽可能精准停靠在终点线。

<p align="left">
<img alt="MCU" src="https://img.shields.io/badge/MCU-STM32F103C8T6-blue">
<img alt="IDE" src="https://img.shields.io/badge/IDE-Keil%20MDK5-green">
<img alt="HAL" src="https://img.shields.io/badge/Driver-STM32%20HAL-orange">
<img alt="License" src="https://img.shields.io/badge/License-MIT-yellow">
</p>

---

## 项目简介

一台**纯开环步态 + 蓝牙微调**的低成本四足机器人。没有 IMU、没有视觉，靠「8 相位错峰步态 + 赛前标定 + 人工单步补偿」完成直行与精准停靠。

**核心设计**

| 设计 | 说明 |
| --- | --- |
| **8 相位错峰步态** | 一个步态周期拆成 8 个微动作，每次只让 2 个舵机动作，把 8×SG90 的 5 A 峰值压到约 1.5 A，避免 5 V 被拉垮导致 MCU 复位 |
| **非阻塞调度** | 全程 `HAL_GetTick()` 时间片，不用死延时，蓝牙急停 < 10 ms 可打断 |
| **分层状态机** | 宏观任务状态（停 / 前进 / 左转 / 右转）+ 微观相位（0~7） |
| **蓝牙单步微调** | 手机发一个字符走一个完整步态，用于终点毫米级停靠 |
| **电源分级** | 舵机 60 mil 粗干路 + 主控细支路 + 2×1000 µF 蓄水 |

## 硬件构成

| 部件 | 型号 | 数量 |
| --- | --- | --- |
| 主控 | STM32F103C8T6 最小系统板（72 MHz） | 1 |
| 执行器 | SG90 舵机（180°） | 8 |
| 通信 | HC-05 蓝牙模块（9600 8N1） | 1 |
| 显示 | 0.96" OLED（SSD1306，I²C 0x78） | 1 |
| 电源 | 2S 锂电 7.4 V 1500 mAh（XT60） | 1 |
| 降压 | XL4015 模块（7.4 V → 5 V / 5 A） | 1 |
| 电容 | 1000 µF / 16 V 电解 | 2 |
| 传感/交互 | 红外 TCRT5000、有源蜂鸣器、轻触按键 ×2 | 各 1 |
| 结构 | 3D 打印：机身 1 + 腿 ×4 + 脚 ×4 | 1 套 |
| 底板 | 自制 PCB 50 × 80 mm，2 层 | 1 |

完整物料见 [`hardware/BOM.csv`](hardware/BOM.csv)，引脚分配见 [`docs/overview.md`](docs/overview.md#23-引脚分配表最终版)。

## 仓库结构

```
robot2/
├── Core/                  CubeMX 生成的初始化代码
├── HARDWARE/              手写驱动：OLED / 蓝牙 / 按键 / 运动状态
├── Drivers/               ST HAL 库 + CMSIS
├── MDK-ARM/               robot.uvprojx  ← Keil 用这个打开
├── robot.ioc              CubeMX 工程
├── hardware/              电路：Gerber + BOM
├── mechanical/            结构：4 个 STL
├── docs/
│   ├── overview.md        工程总纲（参数、标定、踩坑、答辩）
│   └── firmware.md        固件说明（编译 / 烧录 / 代码地图）
├── README.md
└── LICENSE
```

## 快速开始

### 1. 编译

需要 **Keil MDK5 + ARMCC V5.06（AC5）** 和 `Keil.STM32F1xx_DFP` 器件包。

```
打开 MDK-ARM/robot.uvprojx → F7 编译
```

> 若提示缺文件，先确认 `Options → C/C++ → Include Paths` 含 `../Core/Inc; ../Drivers/...; ../HARDWARE`。
> 也可以用 STM32CubeMX 打开 `robot.ioc` 点 **GENERATE CODE** 重新生成骨架（注意 `main.c` 的步态逻辑是手写覆盖的，重生成前先备份）。

### 2. 烧录

| 方式 | 操作 |
| --- | --- |
| ST-Link | `Options → Debug → ST-Link`，`F8` 下载 |
| 串口 ISP | FlyMcu 烧 `robot.hex`；BOOT0=1 上电烧录，烧完 BOOT0=0 复位 |

### 3. 操作（手机蓝牙串口 App，连 HC-05，9600 8N1）

| 发送 | 动作 |
| --- | --- |
| `1` | 开始直行（走到 `TARGET_STEPS` 自动停） |
| `0` | 急停，立即站立 |
| `2` | 单步微调（走 1 个完整步态，用于终点对准） |
| `3` / `4` | 原地左转 / 右转（持续转，发 `0` 才停） |

上电后 OLED 显示表情，机器人自动站立待命。

## 关键参数（`Core/Src/main.c` 顶部）

| 宏 | 当前值 | 含义 |
| --- | --- | --- |
| `TARGET_STEPS` | 60 | 走完 900 mm 的步数，**需按实测标定** |
| `STEP_DELAY` | 100 ms | 单相位时长，8 相位 = 0.8 s/步 |
| `SIDE_ANGLE` | 20° | 大腿前后摆幅 |
| `TURN_ANGLE` | 25° | 原地转向时髋关节偏移 |

**标定**：把 `TARGET_STEPS` 临时改成 20 → 发 `1` → 量实际位移 `D` → `TARGET_STEPS = round(900 / (D/20))`。
完整流程见 [`docs/overview.md`](docs/overview.md#54-距离标定流程做一次10-分钟)。

## 当前状态与已知问题

- ✅ 站立稳定、8 相位步态可走直线、蓝牙三指令可用
- ⚠️ 长距离会**轻微向右偏**（开环 + 舵机虚位所致），比赛中靠蓝牙 `2` 微调补偿
- [ ] `TARGET_STEPS` 未按实测标定
- [ ] `bt_recv_flag` / `bt_recv_char` 在中断与主循环间共享，**缺 `volatile`**，`-O2` 下可能失效
- [ ] `MOTION_TURN_LEFT/RIGHT` 没有自动退出条件，只能发 `0` 停
- [ ] `Robot_Prepare_Sideways()` 是 150/30 的强不对称姿态，与相位 2/7 的 120/60 基准不一致（跑偏的主要嫌疑）
- [ ] 无 IMU，无姿态闭环

## 文档

| 文件 | 内容 |
| --- | --- |
| [`docs/overview.md`](docs/overview.md) | 赛题规则、系统架构、电源与 PCB、引脚表、参数标定、11 条踩坑、答辩话术 |
| [`docs/firmware.md`](docs/firmware.md) | 固件结构、编译烧录、代码地图 |
| [`hardware/README.md`](hardware/README.md) | Gerber 说明与打样参数 |
| [`mechanical/README.md`](mechanical/README.md) | STL 说明、打印建议、150 mm 尺寸校核 |

## 许可证

代码与文档采用 [MIT License](LICENSE)。

第三方组件：
- `Drivers/` 为 STMicroelectronics 的 STM32F1 HAL 库与 CMSIS，遵循其原始 BSD-3-Clause 许可
- `HARDWARE/OLED*` 基于江协科技（江科大）OLED 示例改写，仅供学习交流

---

*2026 集成电路创新实验室招新校赛 · 陆地仿生机器人挑战赛*
