# 旧工程时序基线（寄存器/源码推断）

版本：`timing-baseline/2026-08-28`  
证据类型：旧工程时钟/外设源码、Keil map、DRV8323R 数据手册；**本轮没有连接目标板，所有“实际波形/WCET”仍待测量。**

## 1. 时钟树

旧 mbed 目标选择外部晶体路径，并按以下参数初始化：

| 时钟 | 推断值 | 来源 |
|---|---:|---|
| HSE | 8 MHz（原理图未标频，实物待确认） | `mbed_config.h` + `system_clock.c` |
| PLLM/N/P/Q | 8 / 360 / 2 / 7 | `system_clock.c` |
| SYSCLK/HCLK | 180 MHz / 180 MHz | PLL 与 AHB /1 |
| PCLK1 | 45 MHz | APB1 /4 |
| APB1 timer clock | 90 MHz | APB prescaler≠1，timer ×2 |
| PCLK2 | 90 MHz | APB2 /2 |
| APB2 timer clock | 180 MHz | APB prescaler≠1，timer ×2 |
| Flash latency | 5 wait states | `HAL_RCC_ClockConfig` |

实物确认方法：将 MCO 输出或固定周期 timer 输出接入计数器/示波器，同时读取 RCC 寄存器快照。若 HSE 不是 8 MHz，本文件所有绝对频率都必须重新计算。

## 2. TIM1、PWM 与快速周期

旧寄存器/常量：

```text
TIM1CLK = 180 MHz
PSC     = 0
ARR     = 0x08CA = 2250
CMS     = 2 (center-aligned mode 2)
RCR     = 1
DT      = 0.000025 s
```

中心对齐载波推断：

```text
fPWM = 180,000,000 / ((PSC + 1) × 2 × ARR)
     = 40,000 Hz
TPWM = 25 µs
```

`RCR=1` 的意图是每个完整上/下计数周期产生一次 Update/IRQ，因此 `TIM1_UP_TIM10_IRQHandler` 标称也是 40 kHz。必须在实物上同时输出 Update 测试脚和一相 PWM，确认没有得到 20 kHz 或 80 kHz 的事件率。

旧 FastPWM/mbed 把通道配置为 PWM Mode 1、active high，而 FOC 用 `CCR = ARR × (1-duty)`；迁移指南的等价目标可能使用 PWM Mode 2。这里冻结的是**外部波形**，不是某一组寄存器名字：中心对齐、40 kHz、安全低/中/高占空的逻辑电平必须与旧硬件需求一致。

Keil map 中：

- `TIM1_UP_TIM10_IRQHandler` 地址 `0x080073E1`；符号体 394 B；所在 code section 472 B；
- `Reset_Handler` 地址 `0x08000359`；image entry `0x080001C5`。

代码大小不能推导 WCET，只用于后续 map 对比。

## 3. ADC 基线

旧配置：

| 项 | 推断 |
|---|---|
| 模式 | `ADC->CCR=0x16`，三重规则同步 |
| 时钟 | ADCPRE=0，即 PCLK2/2 = **45 MHz**（超出迁移合同上限） |
| 分辨率 | 默认 12 bit |
| 采样周期 | PC0/CH10、PC1/CH11、PA0/CH0 均设置为 15 ADC cycles |
| 规则序列 | ADC1 CH10（current A net）、ADC2 CH11（current B net）、ADC3 CH0（bus voltage） |
| 触发 | 在 TIM1 ISR 入口软件设置 ADC1 SWSTART |
| 完成同步 | 无 EOC/OVR 等待；两次 SPI3 传输后直接读 DR |

按 12-bit 转换 12 cycles + 15-cycle sample 粗略推断，每路转换约 27 ADC cycles：

- 旧 45 MHz 下约 0.60 µs，但该时钟超规格，数值不能作为合格器件时序；
- 新目标 22.5 MHz 下约 1.20 µs，仍须以参考手册/Cube 配置和实测数据年龄确认。

旧 ISR 在 SWSTART 后执行两次 16-bit SPI3 事务，只有线速就约 1.422 µs，加上 GPIO/软件开销后通常足以覆盖上述 nominal conversion，但这只是偶然的隐式等待；编译优化、器件 SPI、异常或代码重排都会改变它（D-012）。目标必须使用 TIM1 硬件触发和显式完整快照。

## 4. SPI1 — DRV8323RS

SPI1 源时钟为 PCLK2=90 MHz。mbed 算法选择“不高于请求值的最高硬件频率”；请求 500 kHz 最终只能取 `/256`：

```text
fSPI1 = 90 MHz / 256 = 351,562.5 Hz
16-bit wire time = 16 / fSPI1 ≈ 45.511 µs
```

旧 `spi_write()`：nSCS 拉低后固定等待 10 µs，再进行一个 16-bit 事务，随后立即拉高。因此单次函数至少约 55.5 µs + 软件开销。上层初始化通常另有 100 µs 间隔，fault polling 两次读之间有 10 µs。

DRV8323R 数据手册边界：

| 项 | 器件要求 | 旧推断 |
|---|---:|---:|
| 最大 SCLK | 10 MHz（周期最小 100 ns） | 0.3515625 MHz，通过 |
| SCLK high/low | 各至少 50 ns | 约 1.422 µs，各通过 |
| nSCS setup/hold | 各至少 50 ns | setup 10 µs；hold 需实测软件/GPIO |
| nSCS high between words | 至少 400 ns | 上层通常有延时，但每种路径需逻辑分析仪确认 |
| word | 精确 16 clocks、MSB first、Mode 1 边沿 | 源码一致，待实测 |
| ENABLE→SPI ready | `tREADY` 最大 1 ms | 旧代码仅 100 µs，**不通过最坏值** |

目标先保持 351.5625 kHz 以减少变量；ENABLE 后等待至少 1 ms，再通过寄存器读回和逻辑分析仪确认。

## 5. SPI3 — 双位置传感器

SPI3 源时钟为 PCLK1=45 MHz，请求 25 MHz，mbed 选择 `/2`：

```text
fSPI3 = 45 MHz / 2 = 22.5 MHz
one 16-bit wire time = 0.7111 µs
two sequential reads = 1.4222 µs + CS/GPIO/software overhead
```

普通 `Sample()` 依次拉低 PA15/PD2，各传输一个 16-bit `0x7FFE`，没有显式 CS setup 或 CS-high delay。`DualEncoder()` 的初始化路径则在各 CS 拉低后等待 2 µs。原理图可能装 AS5047P、MA700 或 MA730；在确认实装器件和其数据手册前，22.5 MHz、命令格式、错误位和 CS 时间全部标记为**未批准**。

## 6. CAN1 位时序

mbed 算法用 PCLK1=45 MHz、目标 1 Mbps 计算到：

| 参数 | bxCAN 实际值 |
|---|---:|
| Prescaler | 3 |
| Time quantum | 66.667 ns |
| Sync segment | 1 TQ |
| BS1 | 10 TQ |
| BS2 | 4 TQ |
| Total | 15 TQ/bit |
| Bit rate | 45 MHz / (3×15) = 1 Mbps |
| Sample point | (1+10)/15 = 73.33% |
| SJW | 2 TQ（由 BTR 编码值 1 推断） |

旧源码表中的注释百分比没有把 Sync segment 以同一方式计入，不能直接作为分析仪期望。目标 Cube 配置必须明确写出总 TQ 和采样点；用 CAN 分析仪/示波器确认实际 bit time、采样稳定性和错误计数。

## 7. 快速 ISR 路径

### 7.1 普通 MOTOR 路径的顺序

```text
TIM1 Update
  → ADC software start
  → 主/副编码器各一次 SPI3 读取
  → 直接读 ADC2/ADC1/ADC3 DR
  → 位置/速度和 bus filter
  → timeout check
  → torque_control
  → commutate/FOC
  → timeout++ / counter++
  → 清 TIM1 SR
```

目前只有 25 µs nominal 周期，没有 DWT/示波器的普通路径平均值、P99、最大值或 jitter。

### 7.2 已证明会严重超周期的路径

- 第一次进入 MOTOR 且 `DualEncoder=1` 时，ISR 内执行 10 次 `wait_ms(100)`，约阻塞 1 s；
- REST 的状态变化会从 ISR 调菜单/打印；
- CALIBRATION 会从 ISR 进入长标定；
- ENCODER_MODE 在 ISR 打印；
- UART/printf、动态分配和复杂状态路径可进入实时上下文；
- 主循环在电机运行时擦写同一 Flash bank，可能让取指/ISR 延迟不可控；
- CAN RX 回调中 `printf`，并与快速 ISR 共享状态。

因此旧工程不存在有意义的“全路径 WCET <25 µs”结论。阶段 0 只完成了寄存器推断基线，并明确把 WCET 标为未测；不能把 40 kHz 配置频率当作实时性已经满足。

## 8. 目标测量合同

阶段 1/4 的 instrumentation 至少提供：

| 测量 | 方法 | 最低输出 |
|---|---|---|
| TIM1/ISR 周期 | ISR 入口/出口 GPIO + scope | period、high-time min/avg/P99/max、jitter、样本数 |
| CPU cycles | DWT CYCCNT | 每个固定路径的 min/avg/max 和超时计数 |
| ADC 数据年龄 | TIM1 trigger、ADC EOC/DMA、ISR GPIO | trigger→complete、complete→consume、序号一致性 |
| PWM | 三通道 + Update GPIO | 频率、中心对齐、极性、相位、边界 duty |
| SPI1 | nSCS/SCLK/MOSI/MISO/ENABLE | frequency、edge、setup/hold、word gap、tREADY |
| SPI3 | CS/CS2/SCLK/MOSI/MISO | 每器件时序、总事务时间、错误位 |
| CAN | CANH/CANL + analyzer | bit time、sample setting、error frames、洪泛时 ISR jitter |

硬门槛是所有允许运行路径严格小于 25 µs；推荐发布门槛保留至少 20% 裕量（测得 WCET ≤20 µs），若要采用其他裕量必须由项目负责人书面批准。任何延时、打印、Flash、动态分配、长循环或等待邮箱的路径都不得属于 40 kHz ISR。

## 9. 阶段 0 时序结论

| 对象 | 已有基线 | 不确定性 |
|---|---|---|
| PWM/TIM1 Update | 寄存器推断 40 kHz / 25 µs | 实际事件率、波形极性待测 |
| ADC | 三同步、45 MHz、15-cycle、软件触发 | 超规格；完成/数据年龄未测 |
| SPI1 | 351.5625 kHz，单字线速 45.5 µs | GPIO hold/gap 与实物波形待测 |
| SPI3 | 22.5 MHz，两字线速 1.422 µs | 实装器件未知，全部器件时序待批准 |
| CAN | 1 Mbps，15 TQ，sample 73.33% | 总线实测和错误率待测 |
| 快速 ISR | nominal 40 kHz；map 符号已记录 | 普通 WCET 未测；已知状态路径严重超周期 |

该表满足指南 0.6 对“至少有寄存器推导且不确定项明确”的要求；它**不关闭**硬件实测门禁。
