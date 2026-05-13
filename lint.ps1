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

# Tell clang-tidy that exceptions are enabled. MSVC defaults to
# exceptions-on without /EHsc on the command line, but clang-tidy
# doesn't infer that default, so doctest's REQUIRE macro
# static-asserts on "Exceptions are disabled!" otherwise.
#
# Two driver-mode cases need to work:
#   - clang mode: triggered by compile_commands.json containing
#     clang-style flags (-isystem, -std=c++23). -fexceptions and
#     -fcxx-exceptions are recognized and enable exceptions.
#   - clang-cl mode: triggered by compile_commands.json containing
#     cl-style flags (-external:I, -std:c++latest). The clang-style
#     flags are unknown and ignored with a warning, but exceptions
#     default to on in cl mode so doctest is happy anyway.
#
# Which mode clang-tidy picks varies with both LLVM version and the
# CMake version that emitted the compile DB. Forcing the mode (we
# tried --driver-mode=cl) breaks the side that doesn't match.
$tidyArgs += @('--extra-arg=-fexceptions', '--extra-arg=-fcxx-exceptions')

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
