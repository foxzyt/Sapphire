#!/usr/bin/env bash
# Sapphire test runner.
#
# Usage: tests/run_tests.sh [path-to-interpreter]
#   path-to-interpreter  defaults to ./build/sapphire.exe
#
# Runs every tests/test_*.sp through the interpreter. For each test:
#   - if tests/expected/<name>.out exists, stdout must match it exactly
#     (and the exit code must be 0);
#   - otherwise only the exit code is checked, and the test is reported
#     as missing a golden file.
#
# Exits nonzero if any test fails. Works in the MSYS2 MINGW64 shell and
# on plain Linux.

set -u

BIN="${1:-./build/sapphire.exe}"
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXPECTED_DIR="$TESTS_DIR/expected"
# Some tests may open windows or spawn threads; don't let one hang the run.
TIMEOUT_SECS="${SAPPHIRE_TEST_TIMEOUT:-30}"

if [ ! -x "$BIN" ] && ! command -v "$BIN" >/dev/null 2>&1; then
    echo "error: interpreter not found or not executable: $BIN" >&2
    echo "usage: $0 [path-to-interpreter]" >&2
    exit 2
fi

run_with_timeout() {
    if command -v timeout >/dev/null 2>&1; then
        timeout "$TIMEOUT_SECS" "$BIN" "$1"
    else
        "$BIN" "$1"
    fi
}

pass=0
fail=0
missing_golden=()
results=()

shopt -s nullglob
tests=("$TESTS_DIR"/test_*.sp)
shopt -u nullglob

if [ ${#tests[@]} -eq 0 ]; then
    echo "error: no test_*.sp files found in $TESTS_DIR" >&2
    exit 2
fi

for test_file in "${tests[@]}"; do
    name="$(basename "$test_file" .sp)"
    golden="$EXPECTED_DIR/$name.out"
    actual="$(mktemp)"

    run_with_timeout "$test_file" > "$actual"
    code=$?

    status="PASS"
    note=""
    if [ $code -eq 124 ]; then
        status="FAIL"
        note="timed out after ${TIMEOUT_SECS}s"
    elif [ $code -ne 0 ]; then
        status="FAIL"
        note="exit code $code"
    elif [ -f "$golden" ]; then
        if ! diff -u "$golden" "$actual" > "$actual.diff"; then
            status="FAIL"
            note="output differs from expected/$name.out"
            echo "--- $name: output mismatch ---"
            cat "$actual.diff"
            echo "-------------------------------"
        fi
        rm -f "$actual.diff"
    else
        note="exit code only (no golden file)"
        missing_golden+=("$name")
    fi

    if [ "$status" = "PASS" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
    results+=("$status|$name|$note")
    rm -f "$actual"
done

echo
echo "=========== test summary ==========="
printf '%-6s %-28s %s\n' "RESULT" "TEST" "NOTE"
for line in "${results[@]}"; do
    IFS='|' read -r status name note <<< "$line"
    printf '%-6s %-28s %s\n' "$status" "$name" "$note"
done
echo "===================================="
echo "$pass passed, $fail failed, ${#tests[@]} total"

if [ ${#missing_golden[@]} -gt 0 ]; then
    echo
    echo "warning: ${#missing_golden[@]} test(s) have no golden file in tests/expected/ and were checked by exit code only:"
    for name in "${missing_golden[@]}"; do
        echo "  - $name (create tests/expected/$name.out to lock in its output)"
    done
fi

[ $fail -eq 0 ]
