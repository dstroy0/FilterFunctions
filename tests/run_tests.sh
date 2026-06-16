#!/usr/bin/env bash
# FilterFunctions test runner for Linux/macOS
# Usage: bash run_tests.sh [optional: path to g++]
set -euo pipefail

cd "$(dirname "$0")/.."

CXX_CMD="${CXX:-${1:-g++}}"
FLAGS="-std=c++11 -Wall -Wextra"
FAILURES=0

# Track all binaries so we can delete them at the end
BINS=()

compile() {
    local label="$1"; shift
    local out="$1";   shift
    printf "Compiling %-30s ... " "$label"
    if $CXX_CMD $FLAGS "$@" -o "$out" -lm 2>&1; then
        printf "OK\n"
        BINS+=("$out")
    else
        printf "FAILED\n"
        FAILURES=$((FAILURES + 1))
    fi
}

run_suite() {
    local label="$1"
    local bin="$2"
    printf "\n=== %s ===\n" "$label"
    if [ ! -f "$bin" ]; then
        printf "  SKIP (binary not built)\n"
        FAILURES=$((FAILURES + 1))
        return
    fi
    if "./$bin"; then
        printf "PASSED: %s\n" "$label"
    else
        printf "FAILED: %s\n" "$label"
        FAILURES=$((FAILURES + 1))
    fi
}

run_example() {
    local label="$1"
    local bin="$2"
    if [ ! -f "$bin" ]; then
        printf "  SKIP %s (binary not built)\n" "$label"
        return
    fi
    if "./$bin" > /dev/null 2>&1; then
        printf "  %s: OK\n" "$label"
    else
        printf "  %s: FAILED\n" "$label"
        FAILURES=$((FAILURES + 1))
    fi
}

echo "=============================="
echo " FilterFunctions Test Runner  "
echo "=============================="
printf "Compiler: %s\n\n" "$CXX_CMD"

# --- compile ---
compile "filter unit tests"    "test_filters_bin"    \
    tests/test_filters.cpp src/FilterFunctions.cpp

compile "example integration"  "test_examples_bin"   \
    tests/test_examples.cpp src/FilterFunctions.cpp

compile "basic example"        "example_basic_bin"   \
    tests/basic_filters.cpp src/FilterFunctions.cpp

compile "advanced example"     "example_advanced_bin" \
    tests/advanced_filters.cpp src/FilterFunctions.cpp

compile "expert example"       "example_expert_bin"  \
    tests/expert_filters.cpp src/FilterFunctions.cpp

# --- run test suites ---
run_suite "Filter Unit Tests"   "test_filters_bin"
run_suite "Example Tests"       "test_examples_bin"

# --- verify examples run without error ---
printf "\n--- Example smoke tests ---\n"
run_example "basic_filters"    "example_basic_bin"
run_example "advanced_filters" "example_advanced_bin"
run_example "expert_filters"   "example_expert_bin"

# --- cleanup ---
printf "\nRemoving binaries: "
for b in "${BINS[@]}"; do
    rm -f "$b"
    printf "%s " "$b"
done
printf "\n"

# --- report ---
printf "\n"
if [ "$FAILURES" -gt 0 ]; then
    printf "RESULT: %d step(s) FAILED\n" "$FAILURES"
    exit 1
else
    printf "RESULT: All steps PASSED\n"
fi
