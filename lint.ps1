# Run clang-tidy over engine/, platform/, and games/ using the
# `debug` preset's compile_commands.json.
#   ./lint.ps1        — diagnose only
#   ./lint.ps1 -Fix   — apply clang-tidy's suggested fixes
#
# CMakePresets.json sets binaryDir per preset, so the database lives at
# build/debug/compile_commands.json. If it's missing this script
# configures it for you via `cmake --preset debug`. Third-party libs
# (Volk, VMA, doctest) are attached via SYSTEM include directories, so
# their headers are excluded from diagnostics automatically.

[CmdletBinding()]
param(
    [switch]$Fix
)

$ErrorActionPreference = 'Stop'
Set-Location -Path $PSScriptRoot

if (-not (Get-Command clang-tidy -ErrorAction SilentlyContinue))
{
    Write-Error "clang-tidy not found on PATH. Install LLVM (e.g. ``choco install llvm``) or add the Vulkan SDK bin dir to PATH."
    exit 127
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue))
{
    Write-Error "cmake not found on PATH."
    exit 127
}

$buildDir = 'build/debug'
if (-not (Test-Path -Path (Join-Path $buildDir 'compile_commands.json')))
{
    Write-Host "configuring $buildDir for clang-tidy (compile_commands.json missing)..."
    & cmake --preset debug | Out-Null
    if ($LASTEXITCODE -ne 0)
    {
        Write-Error "cmake configure failed"
        exit $LASTEXITCODE
    }
}

$roots = @('engine\src', 'engine\tests', 'platform\src', 'games')
$patterns = @('*.cpp', '*.cc', '*.cxx')

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

$tidyArgs = @('-p', $buildDir, '--quiet')

# doctest's REQUIRE macro static-asserts on "Exceptions are disabled!"
# unless clang-tidy knows exceptions are on. How we tell it depends on
# the driver mode clang-tidy picks (which itself depends on LLVM
# version + the CMake generator that emitted the compile DB):
#
#   - clang-cl mode (MSVC compile DBs with /std:, /EHsc): /EHsc in the
#     compile command already enables exceptions, so no extra flag is
#     needed. Passing -fexceptions here is "unknown argument" — silently
#     ignored on LLVM <=20, but promoted to a hard error on LLVM 22+,
#     which breaks the lint job entirely. Don't pass it.
#   - clang mode (clang-style DBs with -std=, -isystem): /EHsc is not
#     recognized, so we have to pass -fexceptions / -fcxx-exceptions
#     ourselves.
#
# Forcing one mode with --driver-mode breaks the other side, so detect
# from the compile DB shape instead. cl-style flags use `/std:` /
# `/EHsc`; clang-style uses `-std=` / `-isystem`.
$compileDbContent = Get-Content -Raw -Path (Join-Path $buildDir 'compile_commands.json')
if ($compileDbContent -match '"-std=')
{
    $tidyArgs += @('--extra-arg=-fexceptions', '--extra-arg=-fcxx-exceptions')
}

# Suppress driver-mode noise that LLVM 22+ promotes to errors. The
# compile DB contains flags that clang-cl parses but doesn't always
# use during analysis (e.g. /Zc:preprocessor) — older LLVMs ignored
# silently, newer ones emit `error: argument unused during compilation`,
# which `WarningsAsErrors: '*'` then keeps as an error. These are
# meta-diagnostics about driver invocation, not code quality, and
# they fire from clang-tidy startup before any per-TU check filter
# can suppress them. -Wno-* at the driver level is the only knob that
# actually works.
$tidyArgs += @('--extra-arg=-Wno-unused-command-line-argument')

if ($Fix)
{
    $tidyArgs += @('--fix', '--fix-errors')
}

# clang-tidy prints progress (`[i/N] Processing file ...`) and
# `<N> warnings generated.` banners to stderr even with --quiet. The
# banners count diagnostics fired in system/SDK headers, which
# HeaderFilterRegex already excludes from being *reported* — so a clean
# lint can still show large banner numbers. That's expected noise, not a
# regression. Exit code is what matters: 0 means no diagnostics on user
# code.
& clang-tidy @tidyArgs $files
exit $LASTEXITCODE
