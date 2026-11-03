# Dot-source to import the x86 MSVC environment (vcvars32) and VS's CMake + Ninja into this PowerShell session.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x86 toolset not found (install VS Build Tools with C++).' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$env:PATH = "$(Split-Path $vswhere);$env:PATH"   # vcvars itself calls vswhere
cmd /c "`"$vcvars`" >nul 2>nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$cmakeDir\CMake\bin;$cmakeDir\Ninja;$env:PATH"
