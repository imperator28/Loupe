[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
foreach ($command in @('cmake', 'ninja')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Missing required dependency: $command" }
}
if (-not $env:VCPKG_ROOT) { throw 'Missing required dependency: VCPKG_ROOT environment variable' }
if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'))) { throw 'VCPKG_ROOT does not contain scripts/buildsystems/vcpkg.cmake' }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Missing required dependency: MSVC cl.exe; run from a Visual Studio developer shell' }
Write-Output 'Windows native build prerequisites are available.'
