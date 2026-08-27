[CmdletBinding()]
param(
    [string]$SourceRoot,
    [string]$OutputCsv,
    [string]$SummaryJson,
    [string]$FrozenTag = 'legacy-source-baseline-20260828',
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $SourceRoot) {
    $SourceRoot = Join-Path $repoRoot 'MIT_Cheetah_DRV8323_5636'
}
if (-not $OutputCsv) {
    $OutputCsv = Join-Path $repoRoot 'Migration\Baseline\source_inventory.csv'
}
if (-not $SummaryJson) {
    $SummaryJson = Join-Path $repoRoot 'Migration\Baseline\source_inventory_summary.json'
}

if ((Test-Path -LiteralPath $OutputCsv -PathType Leaf) -and
    (Test-Path -LiteralPath $SummaryJson -PathType Leaf) -and
    -not $Force) {
    $existingSummary = Get-Content -LiteralPath $SummaryJson -Raw | ConvertFrom-Json
    $existingCsvHash = (Get-FileHash -LiteralPath $OutputCsv -Algorithm SHA256).Hash
    $currentFrozenCommit = (& git -C $repoRoot rev-list -n 1 $FrozenTag).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $currentFrozenCommit) {
        throw "Cannot resolve frozen Git tag '$FrozenTag'."
    }
    if ($existingCsvHash -ne $existingSummary.csvSha256 -or
        $existingSummary.frozenCommit -ne $currentFrozenCommit -or
        $existingSummary.unclassifiedCount -ne 0) {
        throw 'Existing inventory failed its integrity check. Investigate before using -Force.'
    }
    Write-Host 'Existing immutable baseline inventory passed its integrity check.'
    Write-Host "CSV:    $OutputCsv"
    Write-Host "SHA256: $existingCsvHash"
    return
}

$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$outputParent = Split-Path -Parent $OutputCsv
$summaryParent = Split-Path -Parent $SummaryJson
New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
New-Item -ItemType Directory -Path $summaryParent -Force | Out-Null

# A forced regeneration is only valid while the tracked legacy tree is still
# byte-identical to the frozen tag.  D-001 is deliberately applied afterwards.
& git -C $repoRoot diff --quiet $FrozenTag -- 'MIT_Cheetah_DRV8323_5636'
if ($LASTEXITCODE -ne 0) {
    throw "Tracked legacy files differ from '$FrozenTag'; refusing to regenerate the immutable source inventory."
}

function Get-Classification {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = $RelativePath.Replace('/', '\')
    $lower = $path.ToLowerInvariant()

    if ($lower -eq 'build' -or $lower.StartsWith('build\')) {
        return @{
            Category = '吸收'
            Target = 'Migration/Baseline/Evidence/build（只保留日志、map、axf 摘要；其余重建）'
            Reason = 'Keil 生成物，不进入新工程源码；本清单记录原始文件哈希。'
        }
    }

    if ($lower -eq 'mbed-dev' -or $lower.StartsWith('mbed-dev\') -or
        $lower -eq 'mbed-dev.lib' -or $lower -eq 'fastpwm' -or
        $lower.StartsWith('fastpwm\') -or $lower -eq 'fastpwm.lib' -or
        $lower -eq 'startup_stm32f446xx.s' -or $lower -eq 'debugconfig' -or
        $lower.StartsWith('debugconfig\') -or $lower -eq 'rte' -or
        $lower.StartsWith('rte\')) {
        return @{
            Category = 'HAL/CubeMX 替代'
            Target = 'CubeMX/HAL/CMSIS 生成层'
            Reason = '旧 mbed、FastPWM、启动或 IDE 生成层；行为合同吸收后由 CubeMX/HAL 替代。'
        }
    }

    if ($lower -eq 'eventrecorderstub.scvd' -or $lower -like '*.uvguix.*' -or
        $lower -eq 'jlinklog.txt') {
        return @{
            Category = '废弃'
            Target = '不进入新工程'
            Reason = '本机 IDE/调试会话或日志文件，不构成产品行为。'
        }
    }

    if ($lower -eq 'tables' -or $lower.StartsWith('tables\')) {
        return @{
            Category = '未使用待确认'
            Target = '阶段 4 前由引用扫描和台架确认'
            Reason = '当前引用扫描未发现运行时入口；在删除前保留证据。'
        }
    }

    if ($lower -eq 'flashwriter\stm32f4xx_flash.c' -or
        $lower -eq 'flashwriter\stm32f4xx_flash.h') {
        return @{
            Category = 'HAL/CubeMX 替代'
            Target = 'Storage/HAL Flash 驱动'
            Reason = '旧 SPL Flash 实现由 HAL Flash API 替代。'
        }
    }

    if ($lower -eq 'hw_setup.cpp' -or $lower -eq 'hw_setup.h' -or
        $lower -eq 'preferencewriter' -or $lower.StartsWith('preferencewriter\') -or
        $lower -eq 'flashwriter' -or $lower.StartsWith('flashwriter\') -or
        $lower -eq '.mbed' -or $lower -eq 'mbed_config.h' -or
        $lower -eq 'hobbyking_cheetah_compact_drv8323.uvprojx' -or
        $lower -eq 'hobbyking_cheetah_compact_drv8323.uvoptx' -or
        $lower -eq 'jlinksettings.ini') {
        return @{
            Category = '吸收'
            Target = 'BSP/Storage/新工程配置与阶段 0 合同'
            Reason = '保留功能、资源或构建语义，不逐行照搬旧实现。'
        }
    }

    $migrationRoots = @(
        'calibration', 'can', 'config', 'drv8323', 'fastmath', 'foc',
        'positionsensor'
    )
    foreach ($root in $migrationRoots) {
        if ($lower -eq $root -or $lower.StartsWith("$root\")) {
            $reason = '产品逻辑，迁移时保持外部行为并逐项验证。'
            if ($root -eq 'positionsensor') {
                $reason += ' 绝对编码器路径迁移；同文件内 ABZ 试验路径暂缓启用。'
            }
            return @{
                Category = '迁移'
                Target = 'App/MotorControl/Drivers'
                Reason = $reason
            }
        }
    }

    if ($lower -eq 'main.cpp' -or $lower -eq 'math_ops.cpp' -or
        $lower -eq 'math_ops.h' -or $lower -eq 'structs.h') {
        return @{
            Category = '迁移'
            Target = 'App/MotorControl/Protocol'
            Reason = '产品入口、算法辅助或共享数据定义；按阶段拆分迁移。'
        }
    }

    throw "Unclassified legacy path: $RelativePath"
}

$tracked = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
$gitPaths = & git -C $repoRoot ls-tree -r --name-only $FrozenTag -- 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Cannot read frozen Git tag '$FrozenTag'."
}
foreach ($gitPath in $gitPaths) {
    [void]$tracked.Add($gitPath.Replace('\', '/'))
}

$items = Get-ChildItem -LiteralPath $source -Force -Recurse |
    Sort-Object @{ Expression = { $_.FullName.ToLowerInvariant() } }

$rows = foreach ($item in $items) {
    $relative = $item.FullName.Substring($source.Length).TrimStart('\')
    $classification = Get-Classification -RelativePath $relative
    $repoRelative = $item.FullName.Substring($repoRoot.Length).TrimStart('\').Replace('\', '/')
    $isFile = -not $item.PSIsContainer

    [pscustomobject][ordered]@{
        Path = $relative.Replace('\', '/')
        ItemType = $(if ($isFile) { 'File' } else { 'Directory' })
        Category = $classification.Category
        Target = $classification.Target
        Reason = $classification.Reason
        TrackedAtFrozenTag = $(if ($isFile) { $tracked.Contains($repoRelative) } else { $null })
        Bytes = $(if ($isFile) { $item.Length } else { $null })
        SHA256 = $(if ($isFile) { (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash } else { $null })
        LastWriteTimeUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

$rows | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding utf8

$fileRows = @($rows | Where-Object ItemType -eq 'File')
$directoryRows = @($rows | Where-Object ItemType -eq 'Directory')
$categoryCounts = [ordered]@{}
foreach ($group in ($rows | Group-Object Category | Sort-Object Name)) {
    $categoryCounts[$group.Name] = $group.Count
}

$summary = [ordered]@{
    schemaVersion = 1
    generatedAt = (Get-Date).ToUniversalTime().ToString('o')
    sourceRoot = $source
    frozenTag = $FrozenTag
    frozenCommit = (& git -C $repoRoot rev-list -n 1 $FrozenTag).Trim()
    itemCount = @($rows).Count
    fileCount = $fileRows.Count
    directoryCount = $directoryRows.Count
    totalBytes = ($fileRows | Measure-Object Bytes -Sum).Sum
    trackedFileCount = @($fileRows | Where-Object TrackedAtFrozenTag -eq $true).Count
    untrackedFileCount = @($fileRows | Where-Object TrackedAtFrozenTag -eq $false).Count
    unclassifiedCount = 0
    categoryCounts = $categoryCounts
    csvSha256 = (Get-FileHash -LiteralPath $OutputCsv -Algorithm SHA256).Hash
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $SummaryJson -Encoding utf8

Write-Host "Inventory written: $OutputCsv"
Write-Host "Summary written:   $SummaryJson"
Write-Host "Items=$($summary.itemCount) Files=$($summary.fileCount) Directories=$($summary.directoryCount) Unclassified=0"
