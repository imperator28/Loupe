[CmdletBinding()]
param(
    [string]$EvidencePath = 'docs/evidence/platform/windows.json'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $root 'scripts/bootstrap/windows.ps1')
$rows = @()
foreach ($preset in @('windows-debug', 'windows-release')) {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Configure failed: $preset" }
    cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $preset" }
    ctest --preset $preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed: $preset" }
    $rows += [ordered]@{ preset = $preset; configured = $true; built = $true; testsPassed = $true }
}
$directory = Split-Path -Parent $EvidencePath
if ($directory) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$compiler = (& cl.exe 2>&1 | Select-Object -First 1).ToString()
$ErrorActionPreference = $savedErrorActionPreference
[ordered]@{
    platform = 'windows-x64'; architecture = $env:PROCESSOR_ARCHITECTURE; compiler = $compiler;
    commit = (git -C $root rev-parse HEAD).Trim(); generatedAtUtc = [DateTime]::UtcNow.ToString('o'); presets = $rows
} | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 $EvidencePath
Write-Output "Wrote verification evidence: $EvidencePath"
