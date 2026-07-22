# Contributing to Sapphire

Thanks for your interest in contributing! This guide covers building the
project and running its tests. For bug reports and feature ideas, please
use the issue templates.

## Building from source (Windows / MSYS2)

These are the same steps used by CI and the release builds
(kept in sync with [release-footer.md](release-footer.md)):

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 MINGW64** terminal and install the dependencies:
   ```bash
   pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-sfml mingw-w64-x86_64-openssl
   ```
3. Clone the repository:
   ```bash
   git clone https://github.com/foxzyt/Sapphire.git
   cd Sapphire
   ```
4. Configure and build:
   ```bash
   cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DSFML_DIR=/mingw64/lib/cmake/SFML -DSFML_STATIC_LIBRARIES=ON -DOPENSSL_ROOT_DIR=/mingw64 -DOPENSSL_USE_STATIC_LIBS=TRUE
   cmake --build build --config Release -j 8
   ```

The interpreter ends up at `build/sapphire.exe`.

## Running the tests

From the repository root (in the MSYS2 MINGW64 terminal or any bash):

```bash
bash tests/run_tests.sh                       # uses ./build/sapphire.exe
bash tests/run_tests.sh path/to/sapphire.exe  # or point at another binary
```

The runner executes every `tests/test_*.sp` file. If a matching golden
file exists in `tests/expected/`, the test's stdout must match it
exactly; otherwise only the exit code is checked (and the runner warns
about the missing golden file). See
[tests/expected/README.md](../tests/expected/README.md) for how to
generate golden files.

When adding a feature or fixing a bug, please add a `tests/test_*.sp`
covering it, and check the boxes in the pull request template.
