param([switch]$Simulate, [switch]$Examples)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
$arduino = if ($cli) { $cli.Source } else { "$env:LOCALAPPDATA\TaskfunTools\bin\arduino-cli.exe" }
if (-not (Test-Path $arduino)) { throw 'Arduino CLI is required.' }
$process = [Diagnostics.Process]::GetCurrentProcess()
$process.PriorityClass = 'BelowNormal'
$affinity = $process.ProcessorAffinity.ToInt64() -band 3
if ($affinity) { $process.ProcessorAffinity = [IntPtr]$affinity }
$sketch = Join-Path $PSScriptRoot 'target\TaskfunRegression'
$boards = @(
  @{ Name = 'uno'; Fqbn = 'arduino:avr:uno' },
  @{ Name = 'mega'; Fqbn = 'arduino:avr:mega' },
  @{ Name = 'zero'; Fqbn = 'arduino:samd:arduino_zero_native' }
)
foreach ($board in $boards) {
  $output = Join-Path $root "build\target\$($board.Name)"
  & $arduino compile --fqbn $board.Fqbn --jobs 1 --warnings all --library $root --build-path $output $sketch
  if ($LASTEXITCODE) { throw "$($board.Name) regression firmware build failed." }
}
if ($Examples) {
  foreach ($board in @($boards[0], $boards[2])) {
    foreach ($example in Get-ChildItem (Join-Path $root 'examples') -Directory) {
      $output = Join-Path $root "build\examples\$($board.Name)\$($example.Name)"
      & $arduino compile --fqbn $board.Fqbn --jobs 1 --library $root --build-path $output $example.FullName
      if ($LASTEXITCODE) { throw "$($board.Name) $($example.Name) example build failed." }
    }
  }
}
if ($Simulate) {
  if (-not $env:WOKWI_CLI_TOKEN) { throw 'Configure WOKWI_CLI_TOKEN before requesting simulation.' }
  $cli = Get-Command wokwi-cli -ErrorAction SilentlyContinue
  $wokwi = if ($cli) { $cli.Source } else { "$env:LOCALAPPDATA\TaskfunTools\bin\wokwi-cli.exe" }
  if (-not (Test-Path $wokwi)) { throw 'Wokwi CLI is required.' }
  foreach ($board in @($boards[0], $boards[1])) {
    $elf = Join-Path $root "build\target\$($board.Name)\TaskfunRegression.ino.elf"
    $diagram = if ($board.Name -eq 'mega') { 'diagram-mega.json' } else { 'diagram.json' }
    & $wokwi $sketch --elf $elf --diagram-file $diagram --expect-text 'TASKFUN TESTS PASSED' --fail-text 'FAIL:' --timeout 30000
    if ($LASTEXITCODE) { throw "$($board.Name) simulation failed (exit $LASTEXITCODE)." }
  }
}
