# 旧工程可重复构建基线

## 1. 结论

在仅应用 D-001 后，旧 Keil 工程连续执行两次全量重建，结果均为 **0 errors / 20 warnings**。两次生成的 loadable BIN 完全一致，map 文件也完全一致，因此旧工程已达到阶段 0 所需的可重复构建基线。

AXF 哈希不同，但其可加载 BIN 完全相同；差异来自 AXF 中非加载的调试/构建元数据，不作为固件不确定性处理。

## 2. 工具链与工程配置

| 项 | 基线值 |
|---|---|
| IDE | Keil µVision `5.43.1.0` |
| 工具链 | MDK-ARM Plus `5.43.0.0` |
| 编译器/链接器 | ARM Compiler 5.06 update 5, build 528 |
| Device Pack | `Keil::STM32F4xx_DFP@3.1.1` |
| Device | `STM32F446RETx` |
| Target | `Hobbyking_Cheetah_Compact_DRV8323` |
| 优化/警告级别 | `-O1` / warning level 2（来自 Keil 工程） |
| 当前 IROM 配置 | `0x08000000 + 0x00080000`（512 KiB，存在 D-009） |
| RAM 配置 | `0x20000000 + 0x00020000`（128 KiB） |

修复前状态保存在 [Evidence/pre_rebuild](Evidence/pre_rebuild)：2026-08-26 日志因 `main.cpp` 全角分号产生 2 errors；同时保留了 2026-07-03 的历史成功 map/linker 报告。历史 map 使用的是 ARM Compiler update 6 build 750，不能代替本次工具链基线。

## 3. 两次重建结果

| 指标 | `legacy_rebuild_01` | `legacy_rebuild_02` |
|---|---:|---:|
| 开始时间 | 2026-08-28 02:25:30 +08:00 | 2026-08-28 02:26:22 +08:00 |
| 耗时 | 43.482 s | 36.306 s |
| Keil 退出码 | 1（有警告） | 1（有警告） |
| Errors | 0 | 0 |
| Warnings | 20 | 20 |
| map 大小 | 2,363,505 B | 2,363,505 B |
| map SHA-256 | `C5BC73F722B4AD18ED56ACE55809B4D0E4ED5A043B174CB1557B1C09351AD274` | 相同 |
| BIN 大小 | 92,920 B | 92,920 B |
| BIN SHA-256 | `946B47725B5DC95D66F9757D02B1226710638D839D99E7211CBA75A500500046` | 相同 |
| AXF 大小 | 4,888,372 B | 4,888,372 B |
| AXF SHA-256 | `001980AE23276E468C1007F35C5AFCA02A9141AB0EFFB614D6C43E53BF4B775A` | `EA1707B7422A2D0B94CEC40B6433CB9C07E0A022886494418F788A0C31318233` |

完整命令、时间、工具版本和产物哈希见两个 `*.summary.json` 及 [build_comparison.json](build_comparison.json)。归档的 Keil HTML 日志仅删除了与固件结果无关的本机许可证身份行；摘要中记录的是脱敏副本的 SHA-256，原始 `BUILD/` 日志保持本机忽略。

## 4. 镜像尺寸与关键符号

| 指标 | 值 |
|---|---:|
| Code | 84,244 B |
| RO-data | 8,540 B |
| RW-data | 456 B |
| ZI-data | 12,732 B |
| Total RO | 92,784 B（90.61 KiB） |
| Total RW（执行期 RAM） | 13,188 B（12.88 KiB） |
| Total ROM | 92,920 B（90.74 KiB） |
| 新规划 256 KiB 程序区占用 | 35.45% |
| 128 KiB RAM 静态占用 | 10.06%（不含最坏栈/堆和标定动态分配） |
| Image entry point | `0x080001C5` |
| Load region | base `0x08000000`，size `0x00016C38`，当前 max `0x00080000` |
| `Reset_Handler` | `0x08000359`，Thumb code 8 B |
| `TIM1_UP_TIM10_IRQHandler` | `0x080073E1`，符号体 394 B；所在 code section 472 B |

当前镜像末端远低于 `0x08040000`，所以本次二进制没有覆盖参数区；但链接器仍允许增长到 512 KiB，D-009 仍必须在阶段 1 把上限改为 `0x00040000` 并加入构建后检查。

RAM 百分比也不能证明标定安全：旧标定路径约有 43.5 KiB 动态工作区，且 map 的静态尺寸不包含最坏堆/栈峰值。

## 5. 20 条警告的归并

| 类别 | 次数 | 决策 |
|---|---:|---|
| 文件末尾无换行（含头文件被多个 TU 重复报告） | 8 | 旧基线保留；新工程不得存在 |
| 未使用变量 `v_d_ff`、`v_q_ff`、`cogging_current`、`i` | 4 | 阶段 2/6 按意图删除或接入，不静默照搬 |
| `new/delete` 异常规范不一致 | 6 | 新工程禁用运行期动态分配并统一 C/C++ 运行时策略 |
| ABZ 路径使用已废弃 `InterruptIn::rise` | 1 | D-014；暂缓路径不进入首个目标构建 |
| 链接器 `--keep os_cb_sections` 找不到目标 | 1 | mbed/RTOS 遗留；Cube/HAL 工程不复制 |

“基线可构建”不等于这些警告被批准进入最终固件。Release 门禁仍要求无错误、无未经批准警告。

## 6. 重建方式

```powershell
& .\Migration\Baseline\Tools\Repair-LegacyBuildBlocker.ps1
& .\Migration\Baseline\Tools\Invoke-LegacyBaselineBuild.ps1 -RunName legacy_rebuild_03
& .\Migration\Baseline\Tools\Test-Stage0Baseline.ps1
```

脚本默认使用 `D:\Keil5\UV4\UV4.exe`、`-r` 全量重建和 `-j0`。Keil 对有警告的成功构建返回 1；脚本以解析到的 error 数为准，任何非零错误都会失败。产物必须在本次运行期间刷新，否则脚本拒绝归档旧文件。
