# 冻结后旧工程变更日志

冻结标签：`legacy-source-baseline-20260828`  
规则：旧工程只允许为建立/复核基线而做最小变化；每项必须关联缺陷、记录哈希并重新构建。

## D-001 — 2026-08-28

| 字段 | 记录 |
|---|---|
| 文件 | `MIT_Cheetah_DRV8323_5636/main.cpp` |
| 原因 | `state = MOTOR_MODE` 后的全角分号导致 ARMCC 无法解析，修复前构建为 2 errors |
| 精确变化 | 文件偏移 2419：GBK `A3 BB`（U+FF1B）→ ASCII `3B`（`;`） |
| 修复前大小/哈希 | 22,488 B / `5423712DA6AF6691F3ABDA7E8E1FD4E09CFD86C3EF464AA36345718B53BEB0D1` |
| 修复后大小/哈希 | 22,487 B / `96259FD2D5EC0AC5F0384FCFEC46578DA6B1E59B030519BAEAC96256F86CCE18` |
| 受控脚本 | `Migration/Baseline/Tools/Repair-LegacyBuildBlocker.ps1` |
| 验证 | 两次 Keil 全量重建均 0 errors / 20 warnings；两次 BIN SHA-256 相同 |

除上述单字节语义修复外，旧工程产品源码没有其他变化。
