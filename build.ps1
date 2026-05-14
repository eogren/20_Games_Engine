#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('debug','release','relwithdebinfo','asan','ubsan','analyze')]
    [string]$Preset = 'debug',

    [ValidateSet('configure','build','test','all')]
    [string]$Target = 'all'
)

$ErrorActionPreference = 'Stop'

# cmake --preset reads CMakePresets.json from cwd. Anchor to the script's
# directory so this works regardless of where the caller invoked it from.
Set-Location $PSScriptRoot

# Load the MSVC dev environment if it isn't already loaded. cl.exe on PATH
# is the cheap proxy for "Enter-VsDevShell already ran in this session" —
# avoids re-entering and re-appending paths on every invocation.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere - install Visual Studio 2022+ (vswhere ships with the VS installer)."
    }
    $vsPath = & $vswhere -latest -property installationPath
    if (-not $vsPath) {
        throw "vswhere found no Visual Studio installation."
    }

    Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    # -SkipAutomaticLocation: Enter-VsDevShell otherwise cd's into the VS install dir.
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

function Invoke-Step($label, $block) {
    Write-Host "==> $label" -ForegroundColor Cyan
    & $block
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Target -in 'configure','all') {
    Invoke-Step "configure ($Preset)" { cmake --preset $Preset }
    # Mirror the debug preset's compile_commands.json to the repo root so IDE
    # plugins and ad-hoc clang-tidy runs that search upward from source files
    # find it. .clangd at the root points at build/debug directly; this copy
    # covers tools that don't read .clangd.
    if ($Preset -eq 'debug') {
        $ccsrc = Join-Path $PSScriptRoot 'build/debug/compile_commands.json'
        $ccdst = Join-Path $PSScriptRoot 'compile_commands.json'
        if (Test-Path $ccsrc) { Copy-Item -Force $ccsrc $ccdst }
    }
}
if ($Target -in 'build','all')     { Invoke-Step "build ($Preset)"     { cmake --build "build/$Preset" } }
if ($Target -in 'test','all')      { Invoke-Step "test ($Preset)"      { ctest --preset $Preset } }
