# Run clang-format over engine/, platform/, and games/.
#   ./format.ps1          — check only, exits non-zero if anything would change
#   ./format.ps1 -Write   — rewrite files in place
#
# Style comes from .clang-format at the repo root. The legacy cpp/ tree
# has its own .clang-format and is skipped here.

[CmdletBinding()]
param(
    [switch]$Write
)

$ErrorActionPreference = 'Stop'
Set-Location -Path $PSScriptRoot

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue))
{
    Write-Error "clang-format not found on PATH. Install LLVM (e.g. ``choco install llvm``) or add the Vulkan SDK bin dir to PATH."
    exit 127
}

$roots = @('engine\src', 'engine\tests', 'platform\src', 'games')
$patterns = @('*.cpp', '*.h', '*.hpp', '*.cc', '*.cxx')

$files = @()
foreach ($root in $roots)
{
    if (Test-Path -Path $root)
    {
        $files += Get-ChildItem -Path $root -Recurse -File -Include $patterns |
            Where-Object { $_.FullName -notmatch '\\third_party\\' } |
            ForEach-Object { $_.FullName }
    }
}
$files = $files | Sort-Object -Unique

if ($files.Count -eq 0)
{
    Write-Host "no source files found under engine/, platform/, or games/"
    exit 0
}

if ($Write)
{
    & clang-format -i --style=file $files
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "formatted $($files.Count) files"
}
else
{
    & clang-format --dry-run --Werror --style=file $files
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "format check passed ($($files.Count) files)"
}
