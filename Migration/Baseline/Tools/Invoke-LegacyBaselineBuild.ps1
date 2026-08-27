[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunName,

    [string]$Uv4Path = 'D:\Keil5\UV4\UV4.exe',
    [string]$FromElfPath = 'D:\Keil5\ARM\ARMCC\bin\fromelf.exe',
    [string]$TargetName = 'Hobbyking_Cheetah_Compact_DRV8323',
    [switch]$BuildOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$projectRoot = Join-Path $repoRoot 'MIT_Cheetah_DRV8323_5636'
$projectPath = Join-Path $projectRoot 'Hobbyking_Cheetah_Compact_DRV8323.uvprojx'
$buildRoot = Join-Path $projectRoot 'BUILD'
$evidenceRoot = Join-Path $repoRoot 'Migration\Baseline\Evidence\build'
$commandLog = Join-Path $evidenceRoot "$RunName.keil.log"
$summaryPath = Join-Path $evidenceRoot "$RunName.summary.json"
$evidenceProtector = Join-Path $PSScriptRoot 'Protect-BuildEvidence.ps1'

if (-not (Test-Path -LiteralPath $Uv4Path -PathType Leaf)) {
    throw "Keil executable not found: $Uv4Path"
}
if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
    throw "Keil project not found: $projectPath"
}
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null

$mode = if ($BuildOnly) { '-b' } else { '-r' }
$arguments = @(
    $mode,
    $projectPath,
    '-t',
    $TargetName,
    '-j0',
    '-o',
    $commandLog
)

$startedAt = Get-Date
$process = Start-Process -FilePath $Uv4Path `
    -ArgumentList $arguments `
    -WorkingDirectory $projectRoot `
    -WindowStyle Hidden `
    -Wait `
    -PassThru
$finishedAt = Get-Date

if (-not (Test-Path -LiteralPath $commandLog -PathType Leaf)) {
    throw "Keil did not produce its command log: $commandLog (exit code $($process.ExitCode))"
}

$logText = Get-Content -LiteralPath $commandLog -Raw
$errorCount = $null
$warningCount = $null
$summaryMatch = [regex]::Match(
    $logText,
    '(?im)(?<errors>\d+)\s+Error\(s\),\s+(?<warnings>\d+)\s+Warning\(s\)'
)
if ($summaryMatch.Success) {
    $errorCount = [int]$summaryMatch.Groups['errors'].Value
    $warningCount = [int]$summaryMatch.Groups['warnings'].Value
}

$artifactDefinitions = @(
    @{ Source = 'Hobbyking_Cheetah_Compact_DRV8323.map'; Destination = "$RunName.map" },
    @{ Source = 'Hobbyking_Cheetah_Compact_DRV8323.axf'; Destination = "$RunName.axf" },
    @{ Source = 'Hobbyking_Cheetah_Compact_DRV8323.build_log.htm'; Destination = "$RunName.build_log.htm" },
    @{ Source = 'Hobbyking_Cheetah_Compact_DRV8323.htm'; Destination = "$RunName.linker.htm" }
)

$artifacts = @()
foreach ($definition in $artifactDefinitions) {
    $sourcePath = Join-Path $buildRoot $definition.Source
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        continue
    }
    $sourceItem = Get-Item -LiteralPath $sourcePath
    if ($sourceItem.LastWriteTime -lt $startedAt.AddSeconds(-2)) {
        throw "Build artifact was not refreshed by this run: $sourcePath"
    }
    $destinationPath = Join-Path $evidenceRoot $definition.Destination
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    if ($destinationPath.EndsWith('.htm', [StringComparison]::OrdinalIgnoreCase)) {
        & $evidenceProtector -Path $destinationPath | Out-Null
    }
    $destinationItem = Get-Item -LiteralPath $destinationPath
    $artifacts += [ordered]@{
        name = $destinationItem.Name
        bytes = $destinationItem.Length
        sha256 = (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash
        sourceLastWriteTime = $sourceItem.LastWriteTime.ToString('o')
    }
}

$archivedAxf = Join-Path $evidenceRoot "$RunName.axf"
if (Test-Path -LiteralPath $archivedAxf -PathType Leaf) {
    if (-not (Test-Path -LiteralPath $FromElfPath -PathType Leaf)) {
        throw "fromelf executable not found: $FromElfPath"
    }
    $binaryPath = Join-Path $evidenceRoot "$RunName.bin"
    & $FromElfPath --bin --output $binaryPath $archivedAxf | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $binaryPath -PathType Leaf)) {
        throw "fromelf failed to create the loadable binary for $RunName."
    }
    $binaryItem = Get-Item -LiteralPath $binaryPath
    $artifacts += [ordered]@{
        name = $binaryItem.Name
        bytes = $binaryItem.Length
        sha256 = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash
        sourceLastWriteTime = $binaryItem.LastWriteTime.ToString('o')
    }
}

$summary = [ordered]@{
    schemaVersion = 1
    runName = $RunName
    mode = $(if ($BuildOnly) { 'build' } else { 'rebuild' })
    startedAt = $startedAt.ToString('o')
    finishedAt = $finishedAt.ToString('o')
    durationSeconds = [math]::Round(($finishedAt - $startedAt).TotalSeconds, 3)
    uv4Path = $Uv4Path
    uv4FileVersion = (Get-Item -LiteralPath $Uv4Path).VersionInfo.FileVersion
    fromElfPath = $FromElfPath
    project = $projectPath
    target = $TargetName
    command = ('"{0}" {1}' -f $Uv4Path, ($arguments -join ' '))
    exitCode = $process.ExitCode
    errors = $errorCount
    warnings = $warningCount
    commandLogSha256 = (Get-FileHash -LiteralPath $commandLog -Algorithm SHA256).Hash
    artifacts = $artifacts
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "Keil exit code: $($process.ExitCode)"
Write-Host "Errors:         $errorCount"
Write-Host "Warnings:       $warningCount"
Write-Host "Duration:       $($summary.durationSeconds) s"
Write-Host "Evidence:       $evidenceRoot"

if ($null -eq $errorCount) {
    throw 'Could not parse the Keil error/warning summary.'
}
if ($errorCount -ne 0) {
    throw "Legacy build failed with $errorCount error(s)."
}
