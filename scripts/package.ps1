[CmdletBinding()]
param(
    [string]$Generator = 'Visual Studio 18 2026',
    [string]$Architecture = 'x64',
    [string]$BuildType = $(if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { 'Release' }),
    [string]$BuildDir = $(if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'build-package' }),
    [string]$DistDir = $(if ($env:DIST_DIR) { $env:DIST_DIR } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'dist' })
)

$ErrorActionPreference = 'Stop'
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$DistDir = [IO.Path]::GetFullPath($DistDir)
$PackageName = "edb-windows-$($Architecture.ToLowerInvariant())"
$PackageRoot = Join-Path $DistDir $PackageName
$Archive = Join-Path $DistDir "$PackageName.zip"

& (Join-Path $PSScriptRoot 'build.ps1') `
    -Generator $Generator `
    -Architecture $Architecture `
    -BuildType $BuildType `
    -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (Test-Path $PackageRoot) {
    Remove-Item $PackageRoot -Recurse -Force
}
if (Test-Path $Archive) {
    Remove-Item $Archive -Force
}
New-Item $DistDir -ItemType Directory -Force | Out-Null

cmake --install $BuildDir --config $BuildType --prefix $PackageRoot
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Compress-Archive -Path $PackageRoot -DestinationPath $Archive -Force
Remove-Item $PackageRoot -Recurse -Force

Write-Output $Archive
