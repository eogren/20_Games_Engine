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
#
# When LLVM's `run-clang-tidy` (a Python helper that ships in the same
# bin/ dir as clang-tidy) and a real `python` are both available, the
# script fans translation units across cores via `run-clang-tidy -j` and
# prints diagnostics grouped per file. Otherwise it falls back to a
# serial `clang-tidy` invocation with identical behavior.

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
$needsExceptionFlags = $compileDbContent -match '"-std='

# Detect the parallel runner. `run-clang-tidy` ships next to clang-tidy
# in LLVM bin/ as a Python script (no .exe). Requires a working `python`
# on PATH — and we verify that by actually invoking it, since the
# Microsoft Store ships a `python` stub on Windows that resolves via
# Get-Command but pops an installer dialog when run.
$clangTidyDir = Split-Path -Parent (Get-Command clang-tidy).Source
$runClangTidyPath = Join-Path $clangTidyDir 'run-clang-tidy'

$useParallel = $false
if (Test-Path -Path $runClangTidyPath)
{
    $pythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCmd)
    {
        try
        {
            & python --version *> $null
            if ($LASTEXITCODE -eq 0) { $useParallel = $true }
        }
        catch { }
    }
}

if ($useParallel)
{
    # run-clang-tidy uses LLVM-style single-dash long options. It matches
    # positional `files` args as regex against each compile-DB entry's
    # path *after* normalizing to native separators — so on Windows we
    # leave the backslashes alone and just Regex.Escape() each path. A
    # forward-slash regex (matching the DB's stored "file" string) won't
    # match because the comparison happens post-normalization.
    $fileRegexes = $files | ForEach-Object { [Regex]::Escape($_) }

    $rctArgs = @(
        $runClangTidyPath,
        '-p', $buildDir,
        '-quiet',
        '-extra-arg=-Wno-unused-command-line-argument'
    )
    if ($needsExceptionFlags)
    {
        $rctArgs += @('-extra-arg=-fexceptions', '-extra-arg=-fcxx-exceptions')
    }
    if ($Fix)
    {
        # run-clang-tidy invokes clang-apply-replacements to merge
        # fixes across parallel TUs touching shared headers, so this
        # is safe to combine with -j.
        $rctArgs += '-fix'
    }
    $rctArgs += $fileRegexes

    Write-Host "clang-tidy (parallel via run-clang-tidy, $($files.Count) files)"
    & python @rctArgs
    exit $LASTEXITCODE
}
else
{
    # Serial fallback. Identical behavior to pre-parallel script.
    $tidyArgs = @('-p', $buildDir, '--quiet', '--extra-arg=-Wno-unused-command-line-argument')
    if ($needsExceptionFlags)
    {
        $tidyArgs += @('--extra-arg=-fexceptions', '--extra-arg=-fcxx-exceptions')
    }
    if ($Fix)
    {
        $tidyArgs += @('--fix', '--fix-errors')
    }

    Write-Host "clang-tidy (serial — install Python to enable parallel run-clang-tidy)"
    # clang-tidy prints `[i/N] Processing file ...` and `<N> warnings
    # generated.` banners to stderr even with --quiet. The banners count
    # diagnostics fired in system/SDK headers, which HeaderFilterRegex
    # already excludes from being *reported* — so a clean lint can still
    # show large banner numbers. That's expected noise, not a regression.
    # Exit code is what matters: 0 means no diagnostics on user code.
    & clang-tidy @tidyArgs $files
    exit $LASTEXITCODE
}
