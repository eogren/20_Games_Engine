# Run clang-tidy over engine/, platform/, and games/ using the existing
# build/ directory's compile_commands.json.
#   ./lint.ps1        — diagnose only
#   ./lint.ps1 -Fix   — apply clang-tidy's suggested fixes
#
# The root CMakeLists.txt sets CMAKE_EXPORT_COMPILE_COMMANDS ON, so any
# Ninja configure (cmake -S . -B build -G Ninja) produces the database
# clang-tidy needs. If build/compile_commands.json is missing this script
# configures it for you. Third-party libs (Volk, VMA, doctest) are
# attached via SYSTEM include directories, so their headers are excluded
# from diagnostics automatically.

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

$buildDir = 'build'
if (-not (Test-Path -Path (Join-Path $buildDir 'compile_commands.json')))
{
    Write-Host "configuring $buildDir for clang-tidy (compile_commands.json missing)..."
    & cmake -S . -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Debug | Out-Null
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

# /EHsc tells clang-tidy's MSVC-compatibility frontend that exceptions
# are enabled. MSVC's compile commands don't include the flag because
# MSVC defaults to exceptions-on at parse time; clang-tidy doesn't
# infer the default, so doctest's REQUIRE macro static-asserts on
# "Exceptions are disabled!" without it.
$tidyArgs += @('--extra-arg=/EHsc')

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
