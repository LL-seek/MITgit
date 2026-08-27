# 硬件资源合同（阶段 0）

版本：`hardware-contract/2026-08-28`  
静态证据：单页原理图 `MIT_MOTOR.pdf`、旧工程源码、Keil 工程、DRV8323R 数据手册 `drv8323r.pdf`。  
当前状态：**原理图/源码交叉核对已完成，实物门禁未关闭。**

## 1. 证据等级

| 标记 | 含义 |
|---|---|
| S+C | 原理图和源码彼此支持，可以作为新工程初始配置，但仍需台架验收 |
| S | 只在原理图看到；旧代码未使用或未证实 |
| C | 只由源码/工程推断；原理图没有给出足够属性 |
| H | 必须由 BOM、实物连续性或仪器测量确认；未确认前不能关闭阶段 0 |

原理图标题、日期和 Rev 字段未填写，也没有随附 BOM/装配变体。因此它不能单独证明当前实物板的版本（D-025）。

## 2. 核心器件、时钟和存储

| 资源 | 静态结论 | 证据等级 | 新工程合同/动作 |
|---|---|---|---|
| MCU | 原理图 `STM32F446RET6`；Keil target `STM32F446RETx`，512 KiB Flash、128 KiB SRAM | S+C | CubeMX 选择 STM32F446RET6；读取实物丝印确认 |
| HSE | 原理图为 PH0/PH1 外部谐振器/晶体，但未标频率；旧 mbed 配置选择 HSE crystal，代码按 8 MHz 配 PLL | C+H | 目标先按 8 MHz；示波器/MCO/时钟寄存器确认实际频率 |
| SYSCLK | 8/8×360/2 = 180 MHz；OverDrive，Flash latency 5 | C | Cube 复现并用 MCO 或计数器确认 |
| AHB/APB | HCLK 180 MHz，PCLK1 45 MHz，PCLK2 90 MHz；APB1/APB2 timer clocks 各 90/180 MHz | C | 作为所有外设时序计算唯一基准 |
| 程序区 | 旧 linker 允许 `0x08000000～0x0807FFFF` | C | 阶段 1 限定为 `0x08000000～0x0803FFFF`，长度 `0x40000` |
| 参数 A/B | 旧数据在 Sector 6 `0x08040000`；新规划 Sector 6/7 A/B | C | Sector 6 `0x08040000～0x0805FFFF`，Sector 7 `0x08060000～0x0807FFFF` |

本次旧镜像实际 Total ROM 为 92,920 B，尚未碰到参数区；风险是旧 linker **允许未来覆盖**，不是本次 BIN 已经覆盖。

## 3. 数字引脚与外设映射

### 3.1 功率 PWM、使能与故障

| 旧逻辑名 | MCU/复用 | 原理图网络 | DRV8323RS 引脚 | 结论/目标命名 | 等级 |
|---|---|---|---|---|---|
| `PIN_U` / `pwm_u` | PA10 / TIM1_CH3 | `PWMA` | `INHC` pin 41 | 旧“U”实际驱动 DRV C 输入；目标底层命名 `PWM_DRV_C` | S+C+H |
| `PIN_V` / `pwm_v` | PA9 / TIM1_CH2 | `PWMB` | `INHB` pin 39 | 目标底层命名 `PWM_DRV_B` | S+C+H |
| `PIN_W` / `pwm_w` | PA8 / TIM1_CH1 | `PWMC` | `INHA` pin 37 | 旧“W”实际驱动 DRV A 输入；目标底层命名 `PWM_DRV_A` | S+C+H |
| `ENABLE_PIN` | PA11 / GPIO | `ENABLE` | `ENABLE` pin 33；图中还连到 INLB/INLC | 无外部下拉在图中可见；目标复位最早期强制低 | S+C+H |
| Fault | MCU 未看到明确连接 | `FAULT`，10 kΩ 上拉到 3.3 V | `nFAULT` pin 28 | 是否真的连接 MCU/测试点待连续性确认；首版不能假定有 EXTI | S+H |

旧 FOC 在 `PHASE_ORDER` 不同值下还会交换 CCR1/CCR2 的 V/W 语义。因此 `U/V/W`、`A/B/C` 和板上相线三套命名不能互相猜测。阶段 1 的 BSP 只使用物理命名；阶段 4/9 通过低压波形和电流注入建立唯一逻辑相映射（D-015/D-023）。

DRV 配置为 3×PWM；旧 PWM 外设初始化是 PWM Mode 1、active high，FOC 再写入 `1-duty`。迁移指南提出 Mode 2 的等价配置，但只有示波器确认极性和 0%/50%/最大占空行为后才能选择等价寄存器组合。

### 3.2 DRV8323RS SPI1

| 信号 | MCU | 原理图/器件 | 旧配置 | 等级 |
|---|---|---|---|---|
| nSCS | PA4 GPIO | `SPI-NSS-DRV` → nSCS pin 32 | active low，写前低 10 µs | S+C |
| SCLK | PA5 SPI1_SCK | `SPI-SCK` → SCLK pin 31 | 16 bit，CPOL=0/CPHA=1，请求 500 kHz | S+C |
| MISO | PA6 SPI1_MISO | `SPI-MISO` → SDO pin 29，10 kΩ 上拉 | 实际约 351.5625 kHz | S+C |
| MOSI | PA7 SPI1_MOSI | `SPI-MOSI` → SDI pin 30 | MSB first | S+C |

DRV8323RS 数据手册规定 SPI 最多 10 MHz、16 个 SCLK、nSCS 高电平字间隔至少 400 ns，SDI 在下降沿采样、SDO 在上升沿更新；因此旧 Mode 1 与器件边沿合同一致。旧 351.5625 kHz 本身满足上限，但 ENABLE 拉高后只等 100 µs 就开始 SPI，而数据手册 `tREADY` 最坏 1 ms，必须修复为至少 1 ms 并实测（D-017）。

原理图没有看见 ENABLE 外部下拉；旧程序进入 `main()` 后先等待约 100 ms 才构造该 GPIO，再等待约 100 ms 才拉高。复位期间 PA11 可能处于高阻，必须在实物测量并评估硬件下拉（D-018）。

### 3.3 位置传感器 SPI3

| 信号 | MCU | 原理图网络 | 旧配置 | 等级 |
|---|---|---|---|---|
| SCK | PC10 / SPI3_SCK | `SCK` | 16 bit，CPOL=0/CPHA=1，请求 25 MHz，推断实际 22.5 MHz | S+C |
| MISO | PC11 / SPI3_MISO | `MISO`，10 kΩ 上拉 | 每周期读两个器件 | S+C |
| MOSI | PC12 / SPI3_MOSI | `MOSI` | 旧命令 `0x7FFE` | S+C |
| CS | PA15 GPIO | `CS`，10 kΩ 上拉 | 主编码器 | S+C |
| CS2 | PD2 GPIO | `CS2`，10 kΩ 上拉 | 副编码器 | S+C |

同一张原理图同时画有两颗 AS5047P（U2/U3）以及 MA700/MA730 选项，它们复用 SPI/CS 网络。仓库没有编码器数据手册和装配 BOM；旧类名 `PositionSensorAM5147`、注释 AS5047P 和命令 `0x7FFE` 不能证明实物装的是哪种器件。确认器件丝印、装配位号、磁体/齿轮和 SPI 时序是阶段 0 的硬阻塞项（D-016）。

### 3.4 CAN、UART 和指示灯

| 功能 | MCU | 原理图 | 旧配置 | 等级 |
|---|---|---|---|---|
| CAN RX | PB8 / CAN1_RX | TJA1042TK-3 RXD；VIO=3.3 V | 1 Mbps standard | S+C |
| CAN TX | PB9 / CAN1_TX | TJA1042TK-3 TXD；VCC=5 V，STB=GND | 1 Mbps standard | S+C |
| CAN 总线 | CANH/CANL | 图中未见板载 120 Ω 终端 | — | S+H |
| USART2 TX/RX | PA2/PA3 | `USART_TX` / `USART_RX` | 921600 baud | S+C |
| LED | PC5 GPIO | `INDICATOR`，串 200 Ω 到 LED | active level 待波形/板上观察 | S+C+H |

CAN 网络必须在台架上确认两端终端、电平、采样点和错误计数。没有板载终端不一定是缺陷，但必须写入系统接线合同。

## 4. 模拟量与电流相映射

| 物理信号 | MCU ADC | 原理图来源 | 旧读取 | 比例/状态 |
|---|---|---|---|---|
| `I_A1` | PC0 / ADC1_CH10 | DRV SOA (`I_A`) 经 22 Ω / 2.2 nF | `controller.adc1_raw`；随后可能被当作 `i_b` 或 `i_c` | S+C+H |
| `I_B1` | PC1 / ADC2_CH11 | DRV SOB (`I_B`) 经 22 Ω / 2.2 nF | `controller.adc2_raw`；随后可能被当作 `i_b` 或 `i_c` | S+C+H |
| `I_C` | PC2 / ADCx_CH12 | DRV SOC (`I_C`) | 旧代码未配置/未读取 | S；首版仍采用两相采样重构，除非需求改变 |
| Bus voltage | PA0 / ADC3_CH0 | +24V 经 150 kΩ / 10 kΩ 分压（1/16） | 三重规则组第三路 | `V_SCALE=3.3/4096×16=0.012890625 V/count` |
| TEMP0 | PA1 ADC-capable | 温度网络/接口 | 未使用 | S；暂缓 |
| TEMP1 | PC3 ADC-capable | 温度网络/接口 | 未使用 | S；暂缓 |

原理图使用 1 mΩ 低侧分流和 DRV CSA gain 40 V/V；旧比例为：

```text
I_SCALE = 3.3 V / (4096 count × 0.001 Ω × 40 V/V)
        = 0.02014160156 A/count
```

该比例只在分流阻值、CSA 增益、VREF、ADC 满量程和偏置都与实物一致时成立。实际装配阻值和 DRV 寄存器回读必须确认。

旧 FOC 并不把 `ADC1=phase A`、`ADC2=phase B` 直接使用：根据 `PHASE_ORDER` 把两路赋给 `i_b/i_c`，再以 `i_a=-i_b-i_c` 重构。原理图网名、代码注释和控制相名不一致（D-011）。目标映射必须由受控电流注入确认符号和增益。

## 5. ADC/触发合同

旧寄存器 `ADC->CCR=0x16` 选择三重规则同步，但 ADCPRE 保持 `/2`，即 ADC clock 推断为 45 MHz；这超过迁移指南采用的 36 MHz 上限。旧 ISR 软件启动 ADC1，然后依赖两次 SPI3 编码器传输提供隐式等待，最后直接读取 ADC2/1/3 DR，没有 EOC/OVR 同步。

新工程合同：

- PCLK2/4 = 22.5 MHz；
- TIM1 中心对齐事件触发 ADC1/2/3 同步规则转换；
- 明确采样点相对 PWM 中心/边沿的位置；
- DMA/完成标志产生一个带序号的 `AdcSnapshot`，快速循环只消费完整同周期快照；
- 保留 PC0=SOA、PC1=SOB、PA0=bus 的物理来源，不复制错误注释；
- 监测 EOC/OVR、快照丢失和序号跳变。

## 6. Flash 资源合同

| 区域 | 地址 | 决策 |
|---|---|---|
| 程序 | `0x08000000～0x0803FFFF` | linker hard limit 256 KiB；map 后处理检查 |
| 参数 A | Sector 6，`0x08040000～0x0805FFFF` | 版本化记录 A / 旧格式导入源 |
| 参数 B | Sector 7，`0x08060000～0x0807FFFF` | 版本化记录 B |

旧 `FlashWriter.h` 中 `__SECTORS[5]` 错映射到 `FLASH_Sector_6`；虽然当前构造参数为 6，没有触发该索引，但新实现必须按绝对范围验证，不能复制该表（D-019）。

## 7. 已确认的静态冲突/不确定性

1. PA10/PA8 的逻辑 U/W 与 DRV INHC/INHA 交叉；需要波形与相线连续性确认。
2. 电流网名与 FOC 中 `i_b/i_c` 赋值不一致；需要电流注入确认。
3. 同一原理图列出多种编码器器件，缺 BOM/实物身份；SPI3 22.5 MHz 不能在未知器件上先验批准。
4. ENABLE 复位电平、外部下拉和上电瞬态未证明；100 µs 小于 DRV `tREADY` 最大值。
5. nFAULT 网络是否到达 MCU 未证明；当前代码依赖 SPI 轮询且 FSR2 命令有缺陷。
6. ADC 旧时钟超规格，读取完成依赖 SPI 延时。
7. 旧 linker 允许覆盖参数扇区。
8. 原理图无版本/date/BOM，必须把测量绑定到具体板号/序列号。

## 8. 关闭阶段 0 所需的实物复核

所有结果应保存为带板卡序列号、仪器、探头、固件 BIN 哈希、接线和原始截图/CSV 的测试记录。

| ID | 复核 | 通过条件 | 主责 |
|---|---|---|---|
| HW-00 | 板卡身份/BOM | MCU、DRV8323RS、主/副编码器位号与丝印、分流/分压关键阻值可追溯到板版本 | HW |
| HW-01 | 断电连续性 | PA10/9/8 到 DRV INHC/B/A，再到相桥/相线的映射唯一；ENABLE/nFAULT 去向明确 | HW |
| HW-02 | 复位安全 | 受限电源下复位和调试暂停期间 ENABLE、PWM、栅极保持安全；不存在毛刺导通 | HW+FW |
| HW-03 | DRV 唤醒/SPI | ENABLE 上升到首个 nSCS 下降 ≥1 ms；SPI Mode 1、16 clocks、频率和 CS 间隔满足数据手册；寄存器可回读 | FW+QA |
| HW-04 | PWM | TIM1 三路中心对齐 40 kHz；0%、安全中点和最大占空的极性/相位与 3×PWM 预期一致 | FW+HW |
| HW-05 | ADC/电流 | ADC clock 22.5 MHz；触发点稳定；两路注入的符号、增益、偏置和重构相正确；bus scale 校准 | FW+HW+QA |
| HW-06 | 编码器 | 确认器件型号和允许 SCLK；CS/CS2 各自读数、奇偶/错误位、跨圈方向和齿轮比正确 | FW+HW |
| HW-07 | CAN/UART | CAN 1 Mbps 波形、终端、无错误帧；USART2 921600 压测不影响快速循环 | FW+QA |
| HW-08 | 快速路径 | DWT+GPIO 同时测得 ISR WCET、抖动、ADC 数据年龄；所有运行路径 <25 µs 且有批准裕量 | FW+QA |

在 HW-00～HW-08 证据完成前，指南的 0.4 和“引脚、外设和 Flash 资源无未解决冲突”保持未勾选。
