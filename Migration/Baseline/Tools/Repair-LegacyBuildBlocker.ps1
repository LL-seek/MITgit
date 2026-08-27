[CmdletBinding()]
param(
    [string]$SourceFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $SourceFile) {
    $SourceFile = Join-Path $repoRoot 'MIT_Cheetah_DRV8323_5636\main.cpp'
}
$sourcePath = (Resolve-Path -LiteralPath $SourceFile).Path

$expectedBeforeSha256 = '5423712DA6AF6691F3ABDA7E8E1FD4E09CFD86C3EF464AA36345718B53BEB0D1'
$expectedAfterSha256 = '96259FD2D5EC0AC5F0384FCFEC46578DA6B1E59B030519BAEAC96256F86CCE18'
$bytes = [System.IO.File]::ReadAllBytes($sourcePath)
$beforeSha256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
if ($beforeSha256 -eq $expectedAfterSha256) {
    Write-Host "D-001 build-only repair is already present: $sourcePath"
    Write-Host "SHA256: $beforeSha256"
    return
}
if ($beforeSha256 -ne $expectedBeforeSha256) {
    throw "Refusing to edit an unknown main.cpp revision. Expected SHA256 $expectedBeforeSha256, found $beforeSha256."
}

# Preserve the GBK file byte-for-byte.  Replace only the GBK bytes A3 BB
# (FULLWIDTH SEMICOLON) immediately following the ASCII token below.
$prefix = [System.Text.Encoding]::ASCII.GetBytes('state = MOTOR_MODE')
$needle = [byte[]]::new($prefix.Length + 2)
[System.Array]::Copy($prefix, 0, $needle, 0, $prefix.Length)
$needle[$prefix.Length] = 0xA3
$needle[$prefix.Length + 1] = 0xBB

$matches = [System.Collections.Generic.List[int]]::new()
for ($offset = 0; $offset -le ($bytes.Length - $needle.Length); $offset++) {
    $equal = $true
    for ($index = 0; $index -lt $needle.Length; $index++) {
        if ($bytes[$offset + $index] -ne $needle[$index]) {
            $equal = $false
            break
        }
    }
    if ($equal) {
        $matches.Add($offset)
    }
}

if ($matches.Count -ne 1) {
    throw "Expected exactly one fullwidth-semicolon build blocker, found $($matches.Count)."
}

$matchOffset = $matches[0]
$replacement = [byte[]]::new($bytes.Length - 1)
[System.Array]::Copy($bytes, 0, $replacement, 0, $matchOffset + $prefix.Length)
$replacement[$matchOffset + $prefix.Length] = 0x3B
$sourceTailOffset = $matchOffset + $needle.Length
$targetTailOffset = $matchOffset + $prefix.Length + 1
[System.Array]::Copy(
    $bytes,
    $sourceTailOffset,
    $replacement,
    $targetTailOffset,
    $bytes.Length - $sourceTailOffset
)

[System.IO.File]::WriteAllBytes($sourcePath, $replacement)
$afterSha256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash

Write-Host 'Applied D-001 build-only repair.'
Write-Host "File:          $sourcePath"
Write-Host "Byte offset:   $($matchOffset + $prefix.Length)"
Write-Host "Before SHA256: $beforeSha256"
Write-Host "After SHA256:  $afterSha256"
Write-Host "Bytes:         $($bytes.Length) -> $($replacement.Length)"
