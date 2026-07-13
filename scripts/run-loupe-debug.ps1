[CmdletBinding()]
param(
    [ValidateSet("windows-debug", "windows-release")]
    [string]$Preset = "windows-debug"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repositoryRoot "build/$Preset"
$application = Join-Path $buildRoot "loupe-app.exe"
$configurationRoot = if ($Preset -eq "windows-debug") { "debug" } else { "" }
$qtRoot = Join-Path $buildRoot "vcpkg_installed/x64-windows/$configurationRoot/Qt6"
$qtBin = Join-Path $buildRoot "vcpkg_installed/x64-windows/$configurationRoot/bin"

if (-not (Test-Path -LiteralPath $application)) {
    throw "Loupe has not been built for '$Preset': $application"
}
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot "plugins/platforms"))) {
    throw "Qt platform plugins are missing for '$Preset': $qtRoot"
}

$env:QT_PLUGIN_PATH = Join-Path $qtRoot "plugins"
$env:QML2_IMPORT_PATH = Join-Path $qtRoot "qml"
$env:PATH = "$qtBin;$env:PATH"

& $application
exit $LASTEXITCODE
