[CmdletBinding()]
param(
    [string]$BuildDir = $(if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'build-lint' })
)

$ErrorActionPreference = 'Stop'
$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

foreach ($Tool in @('cmake', 'ninja', 'clang-tidy', 'clang-cl', 'cppcheck')) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "$Tool is required but was not found in PATH"
    }
}

cmake -S $RootDir -B $BuildDir -G Ninja `
    -DBUILD_TESTING=ON `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DCMAKE_C_COMPILER=cl `
    -DCMAKE_CXX_COMPILER=clang-cl `
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Sources = @(
    (Join-Path $RootDir 'edb/EDBInterface.cpp'),
    (Join-Path $RootDir 'edb/EDBCLIOptions.cpp'),
    (Join-Path $RootDir 'edb/EDBUtils.cpp'),
    (Join-Path $RootDir 'edb/main.cpp'),
    (Join-Path $RootDir 'edb/CComHelper.cpp'),
    (Join-Path $RootDir 'tests/EDBCLIOptionsTests.cpp'),
    (Join-Path $RootDir 'tests/CComHelperTests.cpp'),
    (Join-Path $RootDir 'tests/EDBUtilsTests.cpp'),
    (Join-Path $RootDir 'tests/EDBInterfaceTests.cpp'),
    (Join-Path $RootDir 'tests/EDBTransportFactoryStub.cpp')
)

Write-Host 'Running clang-tidy'
& clang-tidy -p $BuildDir @Sources
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'Running cppcheck'
$CompileCommands = Join-Path $BuildDir 'compile_commands.json'
& cppcheck "--project=$CompileCommands" `
    --enable=warning,performance,portability `
    --inline-suppr `
    --error-exitcode=1 `
    --suppress=missingIncludeSystem `
    '--suppress=*:*_deps*' `
    --suppress=virtualCallInConstructor `
    '--suppress=unknownMacro:*EDBWinReg.cpp' `
    --quiet
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'Static analysis passed'
