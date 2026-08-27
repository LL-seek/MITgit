[CmdletBinding()]
param(
    [Parameter(Mandatory, ValueFromPipeline, ValueFromPipelineByPropertyName)]
    [Alias('FullName')]
    [string[]]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$licensePrefix = [Text.Encoding]::ASCII.GetBytes('License Information:')
$redactedLine = [Text.Encoding]::ASCII.GetBytes('License Information: [REDACTED - local license and identity]')

function Find-ByteSequence {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)][byte[]]$Needle,
        [int]$StartIndex = 0
    )

    if ($Needle.Length -eq 0 -or $Bytes.Length -lt $Needle.Length) {
        return -1
    }

    for ($index = $StartIndex; $index -le $Bytes.Length - $Needle.Length; $index++) {
        $matches = $true
        for ($needleIndex = 0; $needleIndex -lt $Needle.Length; $needleIndex++) {
            if ($Bytes[$index + $needleIndex] -ne $Needle[$needleIndex]) {
                $matches = $false
                break
            }
        }
        if ($matches) {
            return $index
        }
    }

    return -1
}

function Protect-OneFile {
    param([Parameter(Mandatory)][string]$LiteralPath)

    $resolvedPath = (Resolve-Path -LiteralPath $LiteralPath).Path
    $bytes = [IO.File]::ReadAllBytes($resolvedPath)
    $lineStart = Find-ByteSequence -Bytes $bytes -Needle $licensePrefix
    if ($lineStart -lt 0) {
        return [ordered]@{ path = $resolvedPath; status = 'not-present' }
    }

    $alreadyRedacted = [Text.Encoding]::ASCII.GetString(
        $bytes,
        $lineStart,
        [Math]::Min($redactedLine.Length, $bytes.Length - $lineStart)
    ) -eq [Text.Encoding]::ASCII.GetString($redactedLine)
    if ($alreadyRedacted) {
        return [ordered]@{ path = $resolvedPath; status = 'already-redacted' }
    }

    $lineEnd = $lineStart
    while ($lineEnd -lt $bytes.Length -and $bytes[$lineEnd] -ne 10 -and $bytes[$lineEnd] -ne 13) {
        $lineEnd++
    }

    $stream = [IO.MemoryStream]::new()
    try {
        if ($lineStart -gt 0) {
            $stream.Write($bytes, 0, $lineStart)
        }
        $stream.Write($redactedLine, 0, $redactedLine.Length)
        if ($lineEnd -lt $bytes.Length) {
            $stream.Write($bytes, $lineEnd, $bytes.Length - $lineEnd)
        }
        [IO.File]::WriteAllBytes($resolvedPath, $stream.ToArray())
    }
    finally {
        $stream.Dispose()
    }

    return [ordered]@{ path = $resolvedPath; status = 'redacted' }
}

$results = foreach ($item in $Path) {
    Protect-OneFile -LiteralPath $item
}

$results | ForEach-Object {
    Write-Host "$($_.status): $($_.path)"
}
