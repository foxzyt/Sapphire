## How to Compile (MinGW)

To build Sapphire from source on Windows using MSYS2 and MinGW:

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
