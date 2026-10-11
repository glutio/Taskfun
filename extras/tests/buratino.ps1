# Finds the Buratino library (https://github.com/glutio/Buratino), which
# Taskfun depends on: $env:BURATINO_DIR, a checkout next to this repository,
# or the Arduino sketchbook's libraries folder.
function Find-Buratino($root) {
  $candidates = @($env:BURATINO_DIR, (Join-Path $root '..\Buratino'))
  $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($cli) {
    $sketchbook = (& $cli.Source config get directories.user 2>$null | Select-Object -First 1)
    if ($sketchbook) { $candidates += Join-Path $sketchbook 'libraries\Buratino' }
  }
  $found = $candidates | Where-Object { $_ -and (Test-Path (Join-Path $_ 'src\Buratino.h')) } | Select-Object -First 1
  if (-not $found) { throw 'Buratino library not found; set BURATINO_DIR to its folder.' }
  (Resolve-Path $found).Path
}
