# FilterFunctions test runner for Windows (PowerShell 5+)
# Usage: .\run_tests.ps1 [-Compiler g++]
param(
    [string]$Compiler = ""
)

$ErrorActionPreference = 'Stop'
Set-Location "$PSScriptRoot\.."

# Resolve compiler: param > $env:CXX > g++ in PATH
if ($Compiler -eq "") { $Compiler = if ($env:CXX) { $env:CXX } else { "g++" } }

try { & $Compiler --version 2>&1 | Out-Null }
catch {
    Write-Error "Compiler '$Compiler' not found. Install MinGW-w64 or set -Compiler / `$env:CXX."
    exit 1
}

$flags   = @('-std=c++11', '-Wall', '-Wextra')
$failures = 0
$bins    = [System.Collections.Generic.List[string]]::new()

function Compile-Binary {
    param([string]$Label, [string]$Out, [string[]]$Sources)
    Write-Host ("Compiling {0,-30} ... " -f $Label) -NoNewline
    $args = $flags + $Sources + @('-o', $Out, '-lm')
    & $Compiler @args 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "OK"
        $script:bins.Add($Out)
    } else {
        Write-Host "FAILED"
        $script:failures++
    }
}

function Run-Suite {
    param([string]$Label, [string]$Bin)
    Write-Host ""
    Write-Host ("=== {0} ===" -f $Label)
    if (-not (Test-Path $Bin)) {
        Write-Host "  SKIP (binary not built)"
        $script:failures++
        return
    }
    & ".\$Bin"
    if ($LASTEXITCODE -eq 0) { Write-Host ("PASSED: {0}" -f $Label) }
    else                      { Write-Host ("FAILED: {0}" -f $Label); $script:failures++ }
}

function Run-Example {
    param([string]$Label, [string]$Bin)
    if (-not (Test-Path $Bin)) {
        Write-Host ("  SKIP {0} (binary not built)" -f $Label)
        return
    }
    & ".\$Bin" 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host ("  {0}: OK" -f $Label) }
    else                      { Write-Host ("  {0}: FAILED" -f $Label); $script:failures++ }
}

Write-Host "=============================="
Write-Host " FilterFunctions Test Runner  "
Write-Host "=============================="
Write-Host "Compiler: $Compiler"
Write-Host ""

# --- compile ---
Compile-Binary "filter unit tests"    "test_filters_bin.exe"    `
    @("tests/test_filters.cpp", "src/FilterFunctions.cpp")

Compile-Binary "example integration"  "test_examples_bin.exe"   `
    @("tests/test_examples.cpp", "src/FilterFunctions.cpp")

Compile-Binary "basic example"        "example_basic_bin.exe"   `
    @("tests/basic_filters.cpp", "src/FilterFunctions.cpp")

Compile-Binary "advanced example"     "example_advanced_bin.exe" `
    @("tests/advanced_filters.cpp", "src/FilterFunctions.cpp")

Compile-Binary "expert example"       "example_expert_bin.exe"  `
    @("tests/expert_filters.cpp", "src/FilterFunctions.cpp")

# --- run test suites ---
Run-Suite "Filter Unit Tests"  "test_filters_bin.exe"
Run-Suite "Example Tests"      "test_examples_bin.exe"

# --- verify examples run ---
Write-Host ""
Write-Host "--- Example smoke tests ---"
Run-Example "basic_filters"    "example_basic_bin.exe"
Run-Example "advanced_filters" "example_advanced_bin.exe"
Run-Example "expert_filters"   "example_expert_bin.exe"

# --- cleanup ---
Write-Host ""
Write-Host "Removing binaries:" -NoNewline
foreach ($b in $bins) {
    Remove-Item -Force $b -ErrorAction SilentlyContinue
    Write-Host " $b" -NoNewline
}
Write-Host ""

# --- report ---
Write-Host ""
if ($failures -gt 0) {
    Write-Host "RESULT: $failures step(s) FAILED"
    exit 1
} else {
    Write-Host "RESULT: All steps PASSED"
}
