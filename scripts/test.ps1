[CmdletBinding()]
param(
    [string]$Generator = 'Visual Studio 18 2026',
    [string]$Architecture = 'x64',
    [string]$BuildType = $(if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { 'Debug' }),
    [string]$BuildDir = $(if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'build-tests' })
)

$ErrorActionPreference = 'Stop'
$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

cmake -S $RootDir -B $BuildDir -G $Generator -A $Architecture -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build $BuildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

ctest --test-dir $BuildDir -C $BuildType --output-on-failure
exit $LASTEXITCODE
