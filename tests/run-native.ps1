param([switch]$Sanitize)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$build = Join-Path $root 'build\native'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Microsoft C++ Build Tools are required.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'No MSVC C++ installation found.' }
$process = [Diagnostics.Process]::GetCurrentProcess()
$process.PriorityClass = 'BelowNormal'
$affinity = $process.ProcessorAffinity.ToInt64() -band 3
if ($affinity) { $process.ProcessorAffinity = [IntPtr]$affinity }
$sanitizer = if ($Sanitize) { '/fsanitize=address /Zi' } else { '' }
$command = 'call "{0}\VC\Auxiliary\Build\vcvars64.bat" >nul && cd /d "{1}" && cl /nologo /std:c++14 /permissive- /EHsc /W4 {3} /I"{2}\tests\native" /I"{2}\src" /FIArduino.h "{2}\tests\native\tests.cpp" "{2}\src\BTaskSwitcher.cpp" /Fe:"{1}\tests.exe" && "{1}\tests.exe"' -f $vs, $build, $root, $sanitizer
& $env:ComSpec /d /c $command
if ($LASTEXITCODE) { throw "Native tests failed (exit $LASTEXITCODE)." }
