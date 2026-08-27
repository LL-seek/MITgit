# 旧源码冻结与逐项分类

## 1. 冻结标识

| 字段 | 值 |
|---|---|
| 仓库根目录 | `C:\Users\l3098\OneDrive\Desktop\MITgit` |
| 旧工程目录 | `MIT_Cheetah_DRV8323_5636` |
| 冻结日期 | 2026-08-28（Asia/Shanghai） |
| Git 分支 | `main` |
| 冻结提交 | `122d12ab8c7d3f71e03a5ffa2768257d09d0caa7` |
| 不可移动标签 | `legacy-source-baseline-20260828` |
| 目标 | `Hobbyking_Cheetah_Compact_DRV8323` |
| MCU | `STM32F446RETx`；原理图器件 `STM32F446RET6` |
| Keil 工程 | `Hobbyking_Cheetah_Compact_DRV8323.uvprojx` |
| 工程文件 SHA-256 | `F1D4B054F087C2C230364D7DE5F2A13B318A5AAF098A2A9D310E1EF9612808B7` |

旧目录通过 `.gitattributes` 设置为 `-text`，Git 不得重写换行或编码。尤其 `main.cpp` 是 GBK 文件，冻结提交保存的是原始字节，而不是转码后的文本。

## 2. 清单范围与完整性

[source_inventory.csv](source_inventory.csv) 对旧工程根目录以下的**每个目录和每个文件**建立了一行记录，字段包括：

- 相对路径和项目类型；
- 迁移类别、目标层和分类原因；
- 是否由冻结标签直接跟踪；
- 文件字节数、SHA-256 和最后修改时间（UTC）。

生成结果：

| 指标 | 数值 |
|---|---:|
| 文件 | 1,044 |
| 目录 | 33 |
| 总项数 | 1,077 |
| 总字节数 | 396,928,197 |
| 冻结标签直接跟踪的旧工程文件 | 468 |
| 未跟踪但已哈希的旧工程文件 | 576 |
| 未分类项 | **0** |
| CSV SHA-256 | `D85D86E32490DC5609814842F1C318043505927336288779A9D3C19744999F3D` |

576 个未跟踪文件的组成是 571 个 `BUILD/` 生成物、4 个 `*.uvguix.*` 用户状态文件和 1 个 `JLinkLog.txt`。它们不属于迁移源码，但没有从审计范围中消失。

## 3. 分类结果

下表计数包含目录和文件：

| 类别 | 数量 | 迁移含义 |
|---|---:|---|
| 迁移 | 28 | 产品逻辑按合同迁移到 App、Control、Protocol 或 Device 层 |
| HAL/CubeMX 替代 | 456 | mbed、FastPWM、启动、RTE、调试生成层和旧 SPL，由 CubeMX/HAL/CMSIS 生成层替代 |
| 吸收 | 585 | 不逐行移植；提取构建、资源、Flash、参数或 BSP 语义后重新实现；多数是 `BUILD/` 证据 |
| 废弃 | 6 | 本机 IDE 会话、日志和 EventRecorder 状态，不进入新工程 |
| 未使用待确认 | 2 | `Tables/imax_vs_w.h` 及其目录；当前无运行时引用，删除前仍需阶段 2/6 复核 |

关键目录的处置：

| 旧项 | 分类 | 新目标/说明 |
|---|---|---|
| `main.cpp` | 迁移 | 拆分为 App 状态机、Protocol、MotorHW 和诊断任务 |
| `CAN/` | 迁移 | `Protocol/CAN`；字节协议由阶段 0 合同冻结 |
| `FOC/`、`FastMath/` | 迁移 | 纯 C Control/Common；不携带硬件访问 |
| `DRV8323/` | 迁移 | `Device/DRV8323`；修正寄存器命令和时序 |
| `PositionSensor/` | 迁移 | 绝对编码器路径迁移；同文件内 ABZ 试验路径暂缓启用 |
| `Calibration/` | 迁移 | 非阻塞标定状态机 |
| `hw_setup.*` | 吸收 | 资源和寄存器行为进入 BSP/合同，不复制危险寄存器写法 |
| `PreferenceWriter/` | 吸收 | 参数索引语义进入 `MotorParameters`，实现由 A/B 记录替代 |
| `FlashWriter/stm32f4xx_flash.*` | HAL/CubeMX 替代 | HAL Flash 驱动 |
| `FastPWM/` | HAL/CubeMX 替代 | Cube TIM1 + MotorHW |
| `mbed-dev/`、`mbed-dev.lib` | HAL/CubeMX 替代 | 不进入新工程 |
| `BUILD/` | 吸收 | 仅保存可复核日志/map/BIN；对象文件随时可重建 |

## 4. 冻结后的唯一源码变化

冻结版本 `main.cpp`：

- 字节数：22,488；
- SHA-256：`5423712DA6AF6691F3ABDA7E8E1FD4E09CFD86C3EF464AA36345718B53BEB0D1`；
- `state = MOTOR_MODE` 后为 GBK 字节 `A3 BB`（全角分号）。

为恢复构建，只执行 D-001：把该位置的两个字节 `A3 BB` 替换为一个 ASCII 字节 `3B`。修复后：

- 字节数：22,487；
- SHA-256：`96259FD2D5EC0AC5F0384FCFEC46578DA6B1E59B030519BAEAC96256F86CCE18`。

没有顺手修复其他缺陷，也没有格式化、转码或改写换行。完整登记见 [LEGACY_CHANGELOG.md](LEGACY_CHANGELOG.md)，受控操作见 [Repair-LegacyBuildBlocker.ps1](Tools/Repair-LegacyBuildBlocker.ps1)。

## 5. 复核命令

```powershell
git rev-parse legacy-source-baseline-20260828^{}
git status --short
& .\Migration\Baseline\Tools\New-BaselineInventory.ps1
Get-FileHash .\Migration\Baseline\source_inventory.csv -Algorithm SHA256
```

清单脚本对新增未知路径采用 fail-closed：没有匹配到明确分类规则就抛错，而不是默认归入某个类别。
