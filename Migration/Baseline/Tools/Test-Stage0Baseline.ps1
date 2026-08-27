[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$baselineRoot = Join-Path $repoRoot 'Migration\Baseline'
$evidenceRoot = Join-Path $baselineRoot 'Evidence\build'
$script:assertions = 0

function Assert-True {
    param(
        [Parameter(Mandatory)][bool]$Condition,
        [Parameter(Mandatory)][string]$Message
    )
    if (-not $Condition) {
        throw "Assertion failed: $Message"
    }
    $script:assertions++
}

function Assert-Equal {
    param(
        [AllowNull()]$Actual,
        [AllowNull()]$Expected,
        [Parameter(Mandatory)][string]$Message
    )
    Assert-True -Condition ($Actual -eq $Expected) -Message "$Message (expected '$Expected', actual '$Actual')"
}

function Assert-Bytes {
    param(
        [Parameter(Mandatory)][byte[]]$Actual,
        [Parameter(Mandatory)][byte[]]$Expected,
        [Parameter(Mandatory)][string]$Message
    )
    $same = $Actual.Length -eq $Expected.Length
    if ($same) {
        for ($index = 0; $index -lt $Actual.Length; $index++) {
            if ($Actual[$index] -ne $Expected[$index]) {
                $same = $false
                break
            }
        }
    }
    Assert-True -Condition $same -Message $Message
}

function Convert-FloatToRaw {
    param([double]$Value, [double]$Minimum, [double]$Maximum, [int]$Bits)
    $clamped = [math]::Max($Minimum, [math]::Min($Maximum, $Value))
    return [int][math]::Truncate(($clamped - $Minimum) * ([math]::Pow(2, $Bits) - 1) / ($Maximum - $Minimum))
}

function Convert-RawToFloat {
    param([int]$Raw, [double]$Minimum, [double]$Maximum, [int]$Bits)
    return $Raw * ($Maximum - $Minimum) / ([math]::Pow(2, $Bits) - 1) + $Minimum
}

function Pack-Command {
    param([double]$P, [double]$V, [double]$Kp, [double]$Kd, [double]$T)
    $pRaw = Convert-FloatToRaw $P -12.5 12.5 16
    $vRaw = Convert-FloatToRaw $V -10 10 12
    $kpRaw = Convert-FloatToRaw $Kp 0 500 12
    $kdRaw = Convert-FloatToRaw $Kd 0 5 12
    $tRaw = Convert-FloatToRaw $T -36 36 12
    return [byte[]]@(
        ($pRaw -shr 8),
        ($pRaw -band 0xFF),
        ($vRaw -shr 4),
        ((($vRaw -band 0xF) -shl 4) -bor ($kpRaw -shr 8)),
        ($kpRaw -band 0xFF),
        ($kdRaw -shr 4),
        ((($kdRaw -band 0xF) -shl 4) -bor ($tRaw -shr 8)),
        ($tRaw -band 0xFF)
    )
}

function Pack-Feedback {
    param([byte]$CanId, [double]$P, [double]$V, [double]$T)
    $pRaw = Convert-FloatToRaw $P -12.5 12.5 16
    $vRaw = Convert-FloatToRaw $V -10 10 12
    $tRaw = Convert-FloatToRaw $T -36 36 12
    return [byte[]]@(
        $CanId,
        ($pRaw -shr 8),
        ($pRaw -band 0xFF),
        ($vRaw -shr 4),
        ((($vRaw -band 0xF) -shl 4) -bor ($tRaw -shr 8)),
        ($tRaw -band 0xFF)
    )
}

$frozenCommit = (& git -C $repoRoot rev-parse 'legacy-source-baseline-20260828^{}').Trim()
Assert-Equal $frozenCommit '122d12ab8c7d3f71e03a5ffa2768257d09d0caa7' 'Frozen tag must resolve to the recorded commit'

$legacyDiff = @(& git -C $repoRoot diff --name-only legacy-source-baseline-20260828 -- 'MIT_Cheetah_DRV8323_5636')
Assert-Equal $legacyDiff.Count 1 'Only one tracked legacy file may differ from the frozen tag'
Assert-Equal $legacyDiff[0] 'MIT_Cheetah_DRV8323_5636/main.cpp' 'D-001 must be the only legacy change'

$mainHash = (Get-FileHash -LiteralPath (Join-Path $repoRoot 'MIT_Cheetah_DRV8323_5636\main.cpp') -Algorithm SHA256).Hash
Assert-Equal $mainHash '96259FD2D5EC0AC5F0384FCFEC46578DA6B1E59B030519BAEAC96256F86CCE18' 'D-001 repaired main.cpp hash'

$inventoryPath = Join-Path $baselineRoot 'source_inventory.csv'
$inventorySummary = Get-Content -LiteralPath (Join-Path $baselineRoot 'source_inventory_summary.json') -Raw | ConvertFrom-Json
$inventoryRows = @(Import-Csv -LiteralPath $inventoryPath)
Assert-Equal $inventoryRows.Count 1077 'Inventory row count'
Assert-Equal @($inventoryRows | Where-Object ItemType -eq 'File').Count 1044 'Inventory file count'
Assert-Equal @($inventoryRows | Where-Object ItemType -eq 'Directory').Count 33 'Inventory directory count'
Assert-Equal $inventorySummary.unclassifiedCount 0 'Inventory must have no unclassified entries'
Assert-Equal (Get-FileHash -LiteralPath $inventoryPath -Algorithm SHA256).Hash $inventorySummary.csvSha256 'Inventory hash must match its summary'

$buildComparison = Get-Content -LiteralPath (Join-Path $baselineRoot 'build_comparison.json') -Raw | ConvertFrom-Json
Assert-Equal $buildComparison.runs.Count 2 'Two full rebuilds are required'
foreach ($run in $buildComparison.runs) {
    Assert-Equal $run.errors 0 "$($run.name) error count"
    Assert-Equal $run.warnings 20 "$($run.name) warning count"
}
Assert-True $buildComparison.mapIdentical 'The two map files must be identical'
Assert-True $buildComparison.loadableBinaryIdentical 'The two loadable binaries must be identical'

$map1Hash = (Get-FileHash -LiteralPath (Join-Path $evidenceRoot 'legacy_rebuild_01.map') -Algorithm SHA256).Hash
$map2Hash = (Get-FileHash -LiteralPath (Join-Path $evidenceRoot 'legacy_rebuild_02.map') -Algorithm SHA256).Hash
$bin1Hash = (Get-FileHash -LiteralPath (Join-Path $evidenceRoot 'legacy_rebuild_01.bin') -Algorithm SHA256).Hash
$bin2Hash = (Get-FileHash -LiteralPath (Join-Path $evidenceRoot 'legacy_rebuild_02.bin') -Algorithm SHA256).Hash
Assert-Equal $map1Hash $map2Hash 'Map SHA256 comparison'
Assert-Equal $bin1Hash $bin2Hash 'Loadable BIN SHA256 comparison'
Assert-Equal $bin1Hash '946B47725B5DC95D66F9757D02B1226710638D839D99E7211CBA75A500500046' 'Recorded loadable BIN hash'
Assert-True ($buildComparison.image.totalRomBytes -lt 0x40000) 'Current image must fit below the planned parameter boundary'

Assert-Bytes (Pack-Command -12.5 -10 0 0 -36) ([byte[]](0,0,0,0,0,0,0,0)) 'MIT minimum command vector'
Assert-Bytes (Pack-Command 0 0 0 0 0) ([byte[]](0x7F,0xFF,0x7F,0xF0,0,0,0x07,0xFF)) 'MIT quantized-zero command vector'
Assert-Bytes (Pack-Command 12.5 10 500 5 36) ([byte[]](0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF)) 'MIT maximum command vector'
Assert-Bytes (Pack-Feedback 1 -12.5 -10 -36) ([byte[]](1,0,0,0,0,0)) 'MIT minimum feedback vector'
Assert-Bytes (Pack-Feedback 1 0 0 0) ([byte[]](1,0x7F,0xFF,0x7F,0xF7,0xFF)) 'MIT quantized-zero feedback vector'
Assert-True ([math]::Abs((Convert-RawToFloat 32767 -12.5 12.5 16) - (-0.0001907377737087046)) -lt 1e-12) 'Position quantized-zero decode'

$legacyParameterBase = 0x08040000L
$lastLegacyWordAddress = $legacyParameterBase + (319L * 4L)
Assert-Equal $lastLegacyWordAddress 0x080404FCL 'Legacy parameter word 319 address'
Assert-Equal ($lastLegacyWordAddress + 3L) 0x080404FFL 'Legacy parameter payload end address'
Assert-True (0x0803FFFFL -lt $legacyParameterBase) 'Planned program range must end before parameter Sector 6'

$htmlEvidence = @(Get-ChildItem -LiteralPath (Join-Path $baselineRoot 'Evidence') -Recurse -File -Filter '*.htm')
$unprotectedLicenseLines = @(
    foreach ($htmlFile in $htmlEvidence) {
        $htmlText = Get-Content -LiteralPath $htmlFile.FullName -Raw
        if ($htmlText -match '(?im)LIC=|^License Information:(?![ \t]*\[REDACTED\b)') {
            $htmlFile.FullName
        }
    }
)
Assert-Equal $unprotectedLicenseLines.Count 0 'Tracked HTML evidence must not contain local license credentials'

Write-Host "Stage 0 static baseline checks passed: $script:assertions assertions."
