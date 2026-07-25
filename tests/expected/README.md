# Golden output files

Each `tests/test_<name>.sp` may have a matching `test_<name>.out` here
containing its expected stdout. `tests/run_tests.sh` diffs the actual
output against it and fails on any mismatch. Tests without a golden file
are checked by exit code only (the runner prints a warning listing them).

To (re)generate a golden file from a known-good build:

```bash
./build/sapphire.exe tests/test_<name>.sp > tests/expected/test_<name>.out
```

Review the output before committing it — the golden file defines the
expected behavior from then on.
