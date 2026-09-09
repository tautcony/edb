[CmdletBinding()]
param(
    [string]$Generator = 'Visual Studio 18 2026',
    [string]$Architecture = 'x64',
    [string]$BuildType = $(if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { 'Release' }),
    [string]$BuildDir = $(if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'build' })
)

$ErrorActionPreference = 'Stop'
$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

cmake -S $RootDir -B $BuildDir -G $Generator -A $Architecture
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build $BuildDir --config $BuildType --parallel
exit $LASTEXITCODE
