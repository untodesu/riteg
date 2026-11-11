$ErrorActionPreference = "Stop"
$script = Join-Path $PSScriptRoot "scripts/setup.py"
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $script intellisense @args
} else {
    & python $script intellisense @args
}
exit $LASTEXITCODE
