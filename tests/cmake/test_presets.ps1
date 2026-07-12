$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$presets = Get-Content -Raw (Join-Path $root 'CMakePresets.json') | ConvertFrom-Json
$configure = @($presets.configurePresets.name)
$build = @($presets.buildPresets.name)
$test = @($presets.testPresets.name)

foreach ($name in @('windows-debug', 'windows-release', 'macos-arm64-debug', 'macos-arm64-release')) {
    if ($configure -notcontains $name -or $build -notcontains $name -or $test -notcontains $name) {
        throw "Missing configure/build/test preset: $name"
    }
}

foreach ($name in @('macos-arm64-debug', 'macos-arm64-release')) {
    $preset = @($presets.configurePresets | Where-Object name -eq $name)[0]
    if ($preset.cacheVariables.CMAKE_OSX_ARCHITECTURES -ne 'arm64') {
        throw "Preset $name must target Apple Silicon"
    }
}

Write-Output 'CMake presets contain Windows and Apple Silicon Debug/Release contracts.'
