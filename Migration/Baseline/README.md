# 阶段 0：可追踪基线

基线日期：2026-08-28（Asia/Shanghai）  
当前结论：**静态基线已建立，阶段 0 门禁保持开启；尚缺实物板/BOM/示波器验证，不能进入阶段 1。**

## 1. 当前状态

| 阶段 0 项 | 状态 | 证据 |
|---|---|---|
| 0.1 冻结旧源码 | 完成 | Git 提交 `122d12ab8c7d3f71e03a5ffa2768257d09d0caa7`；标签 `legacy-source-baseline-20260828`；[源码基线](SOURCE_BASELINE.md) |
| 0.2 恢复旧工程构建 | 完成 | 两次全量重建均为 0 errors / 20 warnings；生成的 BIN 与 map 分别逐字节一致；[构建基线](BUILD_BASELINE.md) |
| 0.3 旧文件逐项分类 | 完成 | [逐项清单](source_inventory.csv) 共 1,044 个文件、33 个目录，`unclassifiedCount=0`；[摘要](source_inventory_summary.json) |
| 0.4 硬件资源核对 | **阻塞** | 原理图与源码已交叉核对；实际编码器装配、相线/电流通道、ENABLE 上电状态和波形仍待实物验证；[硬件合同](HARDWARE_RESOURCE_CONTRACT.md) |
| 0.5 冻结协议与参数 | 完成（静态） | [CAN 协议合同](PROTOCOL_CONTRACT.md)、[参数兼容合同](PARAMETER_CONTRACT.md) |
| 0.6 建立时序基线 | 完成（寄存器推断） | [时序基线](TIMING_BASELINE.md)；实际 WCET/波形仍作为硬件复核项 |
| 0.7 缺陷决策表 | 完成 | [缺陷决策](DEFECT_DECISIONS.md)；每项有主责角色、处置阶段和验证方法 |

门禁未关闭的唯一类别是实物证据。未完成项不是文档遗漏，而是必须在目标板上确认的安全事实。

## 2. 基线证据索引

- [SOURCE_BASELINE.md](SOURCE_BASELINE.md)：冻结点、逐文件分类统计、旧源码变更规则。
- [BUILD_BASELINE.md](BUILD_BASELINE.md)：工具链、两次构建、代码/RAM、入口和 ISR 符号、警告债务。
- [HARDWARE_RESOURCE_CONTRACT.md](HARDWARE_RESOURCE_CONTRACT.md)：MCU、时钟、引脚、外设、模拟量、Flash 分区和实物复核清单。
- [PROTOCOL_CONTRACT.md](PROTOCOL_CONTRACT.md)：MIT 命令、反馈、特殊帧、CAN ID、超时及十六进制样例。
- [PARAMETER_CONTRACT.md](PARAMETER_CONTRACT.md)：旧 Sector 6 的二进制布局、默认值、合法范围和 A/B 兼容导入策略。
- [TIMING_BASELINE.md](TIMING_BASELINE.md)：TIM1/PWM、ADC、SPI1、SPI3、CAN 和快速 ISR 的推断值与不确定性。
- [DEFECT_DECISIONS.md](DEFECT_DECISIONS.md)：D-001～D-029 的最终处理决策。
- [LEGACY_CHANGELOG.md](LEGACY_CHANGELOG.md)：冻结后对旧工程的所有变更；当前只有 D-001。
- [source_inventory.csv](source_inventory.csv)：旧工程每个文件/目录的类别、目标、原因、大小、时间和 SHA-256。
- [Evidence/pre_rebuild](Evidence/pre_rebuild)：修复前失败日志及历史成功 map/linker 报告。
- [Evidence/build](Evidence/build)：两次全量构建的命令日志、HTML 日志、map、AXF、BIN 和机器可读摘要。
- [HW_STAGE0_TEMPLATE.md](Evidence/hardware/HW_STAGE0_TEMPLATE.md)：绑定具体板卡、仪器和固件哈希的实物复核记录模板。
- [Tools](Tools)：清单生成、受控 D-001 修复和旧工程构建脚本。

## 3. 可重复执行

在仓库根目录运行：

```powershell
& .\Migration\Baseline\Tools\New-BaselineInventory.ps1
& .\Migration\Baseline\Tools\Repair-LegacyBuildBlocker.ps1
& .\Migration\Baseline\Tools\Invoke-LegacyBaselineBuild.ps1 -RunName legacy_rebuild_03
& .\Migration\Baseline\Tools\Test-Stage0Baseline.ps1
```

清单脚本要求冻结标签存在，并在遇到任何未分类路径时失败；清单生成后默认只校验其不可变哈希。修复脚本只接受已登记的修复前/修复后 SHA-256；不会对未知版本的 `main.cpp` 动手。构建脚本执行 Keil 全量重建，要求刷新 map/AXF/日志，并在错误数非零时失败；归档 HTML 前会调用 `Protect-BuildEvidence.ps1` 删除与构建结果无关的本机许可证身份。静态验收脚本复核冻结点、唯一旧源码变化、清单、两次构建哈希、MIT 协议向量、Flash 边界和证据脱敏，当前通过 30 项断言。

## 4. 冻结规则

1. `legacy-source-baseline-20260828` 永不移动、覆盖或删除。
2. 旧工程只作为参考实现；迁移代码不得继续在旧目录内迭代功能。
3. 必须修改旧工程以恢复证据时，先登记缺陷 ID、修复前后哈希、精确差异和构建证据。
4. `BUILD/`、Keil 用户界面状态和本机日志不进入 Git；它们仍由原始清单保留哈希。需要长期保留的构建证据复制到 `Migration/Baseline/Evidence/`。
   证据副本中的本机许可证身份必须脱敏；脱敏后的文件哈希才是受版本控制的证据哈希。
5. 硬件结论必须区分“原理图确认”“源码推断”和“实物实测”。只有第三类证据可以关闭硬件门禁。

## 5. 下一步（关闭阶段 0 门禁）

需要一块代表量产配置的板卡、对应 BOM/装配变体、示波器或逻辑分析仪，以及受限电源。按 [硬件合同的实物复核清单](HARDWARE_RESOURCE_CONTRACT.md#8-关闭阶段-0-所需的实物复核) 完成记录后，才能把指南中的 0.4 和阶段 0 总门禁勾选为完成。
