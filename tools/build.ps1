param(
    [ValidateSet('windows-x86-debug', 'windows-x86-release', 'windows-x64-debug', 'windows-x64-release')]
    [string]$Preset = 'windows-x86-debug',
    [ValidateSet('All', 'Configure', 'Build', 'Test', 'Install')]
    [string]$Action = 'All',
    [string]$VisualStudioVersion = '[16.0,17.0)',
    [string[]]$ConfigureArguments = @()
)
$ErrorActionPreference = 'Stop'
$sdkRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -version $VisualStudioVersion -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No MSVC installation found for $VisualStudioVersion" }
$arch = if ($Preset -like '*-x86-*') { 'x86' } else { 'x64' }
$devCmd = Join-Path $vsPath 'Common7/Tools/VsDevCmd.bat'
# PATH entries need no quotes; embedded quotes can break the batch initializer.
$env:PATH = $env:PATH.Replace('"', '')
# Import the developer environment into this process; do not print its contents.
$devEnvironment = & $env:ComSpec /d /c "call `"$devCmd`" -no_logo -arch=$arch -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Failed to initialize the MSVC environment.' }
$importedNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($line in $devEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') {
        # Some launchers provide both PATH and Path. cmd updates PATH first;
        # do not overwrite it later with the stale duplicate.
        if ($importedNames.Add($Matches[1])) {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}
# Preset schema v2 supports the CMake 3.20 bundled with VS2019.
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeTooOld = $true
if ($cmakeCommand) {
    $cmakeVersionLine = & $cmakeCommand.Source --version | Select-Object -First 1
    if ($cmakeVersionLine -match 'cmake version (\d+\.\d+\.\d+)') {
        $cmakeTooOld = [version]$Matches[1] -lt [version]'3.20.0'
    }
}
if ($cmakeTooOld -or
    -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $cmakeVsPath = & $vswhere -latest -products '*' -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    $ninjaVsPath = & $vswhere -latest -products '*' -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    if ($cmakeVsPath) { $env:PATH = "$(Split-Path $cmakeVsPath);$env:PATH" }
    if ($ninjaVsPath) { $env:PATH = "$(Split-Path $ninjaVsPath);$env:PATH" }
}
Push-Location $sdkRoot
try {
    if ($Action -in 'All', 'Configure') {
        & cmake --preset $Preset @ConfigureArguments
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    }
    if ($Action -in 'All', 'Build') {
        & cmake --build --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }
    if ($Action -in 'All', 'Test') {
        & ctest --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
        & cmake "-DSDK_BUILD_DIR=$sdkRoot/out/build/$Preset" -P tests/package_consumer/verify.cmake
        if ($LASTEXITCODE -ne 0) { throw 'Installed package verification failed.' }
    }
    if ($Action -eq 'Install') {
        & cmake --install "out/build/$Preset"
        if ($LASTEXITCODE -ne 0) { throw 'Installation failed.' }
    }
}
finally { Pop-Location }
