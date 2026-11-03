<#
.SYNOPSIS
  Build u2sdk (x86, MSVC 2022, Ninja) and optionally run its unit tests.
.EXAMPLE
  .\build.ps1 -Test
#>
param(
  [ValidateSet('Release', 'RelWithDebInfo', 'Debug')] [string]$Config = 'RelWithDebInfo',
  [switch]$Test,
  [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root "build\$Config"
. (Join-Path $root 'tools\msvc.ps1')

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Config" | Out-Host
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build $build | Out-Host
if ($LASTEXITCODE) { throw 'Build failed' }
if ($Test) {
  & (Join-Path $build 'bin\u2sdk_tests.exe') | Out-Host
  if ($LASTEXITCODE) { throw 'Unit tests failed' }
}
