# 硬件设计

## 1. 板卡参数

| 项目 | 参数 |
| --- | --- |
| 板框尺寸 | **50.00 × 79.88 mm**（Gerber GKO 层实测） |
| 层数 | 2 层 |
| 板厚 | 1.6 mm |
| 铜厚 | 1 oz |
| 表面处理 | 有铅/无铅喷锡均可 |
| 特殊孔 | 固定孔为**非金属化 NPTH** |
| 设计软件 | 嘉立创 EDA Pro v3.2.91 |

## 2. Gerber 文件（`gerber/`）

| 文件 | 含义 |
| --- | --- |
| `Gerber_TopLayer.GTL` / `Gerber_BottomLayer.GBL` | 顶层 / 底层铜 |
| `Gerber_TopSilkscreenLayer.GTO` / `Gerber_BottomSilkscreenLayer.GBO` | 顶层 / 底层丝印 |
| `Gerber_TopSolderMaskLayer.GTS` / `Gerber_BottomSolderMaskLayer.GBS` | 顶层 / 底层阻焊 |
| `Gerber_BoardOutlineLayer.GKO` | 板框 |
| `Gerber_DocumentLayer.GDL` / `Gerber_DrillDrawingLayer.GDD` | 文档层 / 钻孔图 |
| `Drill_PTH_Through.DRL` | 金属化孔 |
| `Drill_NPTH_Through.DRL` | 非金属化孔（固定孔） |
| `Drill_PTH_Through_Via.DRL` | 过孔 |
| `FlyingProbeTesting.json` | 飞针测试文件 |

**下单**：把 `gerber/` 整个目录打包上传嘉立创，选 2 层 / 1.6 mm / 1 oz，孔金属化按文件默认即可。

## 3. 电源设计

```
2S 电池 XT60 → 开关 → 2×1000µF ∥ →┬→ XL4015 → 5V → VCC_SERVO（8 舵机，60mil 粗干路）
                                    └─────────→ VCC（MCU/蓝牙/OLED，细支路）
```

- **舵机是绝对的大电流户**：8 个 SG90 同时启动峰值 5 A 以上，干路必须 ≥60 mil。
- **主控支路走细线**，只分小电流；配合 2×1000 µF 电容吸收浪涌。
- 软件侧还有第二重保险：8 相位错峰，任意时刻最多 2 个舵机动作。
- **不要**把 3.3 V 接到 5 V（会烧核心板），核心板自带 3.3 V 稳压。

## 4. PCB 布局要点

- 电源居中/靠边，**舵机接口就近贴在 PWM 输出引脚旁**（PA0~PA3 / PA6、PA7、PB0、PB1），缩短线束。
- 底层整片 GND 铺铜；顶层可不铺。
- 板上的孔用于**螺丝固定**到 3D 打印机身，做非金属化，防止螺丝短接底层地。
- 蓝牙模块天线正下方禁布铜（若用杜邦线外接则无需处理）。

## 5. 接线

完整引脚表见 [`../docs/overview.md`](../docs/overview.md#23-引脚分配表最终版)。要点：

- 舵机信号：TIM2_CH1~4（PA0~PA3）= 舵机 1~4，TIM3_CH1~4（PA6、PA7、PB0、PB1）= 舵机 5~8。
- **舵机电源与 MCU 必须共地**，否则舵机只响不转。
- OLED：PB12(SCL) / PB13(SDA)，CubeMX 必须配成 **Output Open-Drain + Pull-up**。

## 6. BOM

见 [`BOM.csv`](BOM.csv)。价格为待补状态，请按实际采购填写。
