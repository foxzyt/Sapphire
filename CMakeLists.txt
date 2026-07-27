cmake_minimum_required(VERSION 3.11)
project(sapphire C CXX)

if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# Fetch OpenCL Headers
FetchContent_Declare(
    opencl_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
    GIT_TAG        v2024.10.24
)

# Fetch OpenCL ICD Loader
FetchContent_Declare(
    opencl_loader
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
    GIT_TAG        v2024.10.24
)

FetchContent_MakeAvailable(opencl_headers)
set(OPENCL_ICD_LOADER_HEADERS_DIR "${opencl_headers_SOURCE_DIR}" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(opencl_loader)

# Fetch SQLite3 Amalgamation
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${sqlite3_SOURCE_DIR}")

# Fetch SFML
FetchContent_Declare(
    sfml
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        3.0.2
)
# set(SFML_STATIC_LIBRARIES ON)
set(SFML_BUILD_AUDIO OFF)
set(SFML_BUILD_NETWORK ON)
set(SFML_BUILD_GRAPHICS ON)
set(SFML_BUILD_WINDOW ON)
set(SFML_BUILD_SYSTEM ON)
set(BUILD_SHARED_LIBS OFF)
set(ASMJIT_STATIC ON)
FetchContent_MakeAvailable(sfml)

# Use system OpenSSL package (provided by MSYS2/MinGW or locally in libs/openssl)
if(NOT APPLE)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl")
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl")
elseif(APPLE)
    if(EXISTS "/opt/homebrew/opt/openssl@3")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    elseif(EXISTS "/usr/local/opt/openssl@3")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@3")
    elseif(EXISTS "/usr/local/opt/openssl")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    endif()
endif()
find_package(OpenSSL REQUIRED)

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

# Fetch FreeType (required by SFML)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-2
)
FetchContent_MakeAvailable(freetype)

# Fetch AsmJit (for JIT code generation in Rubellite VM)
FetchContent_Declare(
    asmjit
    GIT_REPOSITORY https://github.com/asmjit/asmjit.git
    GIT_TAG        0bd5787b54b575ed94bf32ac452153b34385c514
)
set(ASMJIT_BUILD_X86 ON CACHE BOOL "Build AsmJit X86 backend" FORCE)
set(ASMJIT_BUILD_ARM OFF CACHE BOOL "Disable ARM backend" FORCE)
FetchContent_MakeAvailable(asmjit)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# Compiler-specific optimizations
if(MSVC)
    add_compile_options(/O2 /MT /DNDEBUG)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-O2 -DNDEBUG)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O2 -DNDEBUG)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
endif()

# Static linking for GCC/MinGW on Windows
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Option to enable Rubellite JIT VM (compiled into sapphire.exe)
option(USE_RUBELLITE "Ativar VM experimental Rubellite JIT (compilado dentro do sapphire.exe)" OFF)
if(USE_RUBELLITE)
    add_compile_definitions(USE_RUBELLITE RUBELLITE_ENABLED)
endif()

set(VM_SOURCES
    src/vm/object.cpp
    src/vm/value.cpp
    src/vm/vm.cpp
    src/vm/gc.cpp
    src/vm/ui_retained.cpp
    src/vm/builtins/math.cpp
    src/vm/builtins/string.cpp
    src/vm/builtins/io.cpp
    src/vm/builtins/list.cpp
    src/vm/builtins/map.cpp
    src/vm/builtins/json.cpp
    src/vm/builtins/http.cpp
    src/vm/builtins/thread.cpp
    src/vm/builtins/ui.cpp
    src/vm/builtins/core.cpp
    src/vm/builtins/crypto.cpp
    src/vm/builtins/net.cpp
)
set(VM_INCLUDE_DIR src/vm/)

# Find SFML from FetchContent (redundant with FetchContent_MakeAvailable)
# find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_compile_definitions(HAS_OPENCL)

add_library(sapphire_core STATIC
    src/compiler/compiler.cpp
    src/compiler/debug.cpp
    src/parser/lexer.cpp
    src/parser/parser.cpp
    src/utils/utils.cpp
    src/utils/termcolor.cpp
    src/graphics/engine.cpp
    src/graphics/vec2d.cpp
    src/graphics/vec3d.cpp
    src/compiler/bytecode_io.cpp
    src/api/sapphire_api.cpp
    src/api/opencl_api.cpp
    src/api/sqlite_api.cpp
    src/api/postgres_api.cpp
    src/api/mysql_api.cpp
    src/preprocessor/preprocessor.cpp
    ${VM_SOURCES}
)


# Search for PostgreSQL
find_package(PostgreSQL QUIET)
if(PostgreSQL_FOUND)
    message(STATUS "PostgreSQL found. Enabling PostgreSQL bindings.")
    add_compile_definitions(USE_POSTGRESQL)
    list(APPEND COMMON_LIBS PostgreSQL::PostgreSQL)
else()
    message(WARNING "PostgreSQL NOT found. pgConnect, pgQuery, etc. will not be available.")
endif()

# Search for MySQL / MariaDB
# We will use pkg-config or try to find libmysqlclient
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(MYSQL mysqlclient QUIET)
    if(MYSQL_FOUND)
        message(STATUS "MySQL (pkg-config) found. Enabling MySQL bindings.")
        add_compile_definitions(USE_MYSQL)
        include_directories(${MYSQL_INCLUDE_DIRS})
        link_directories(${MYSQL_LIBRARY_DIRS})
        list(APPEND COMMON_LIBS ${MYSQL_LIBRARIES})
    else()
        # Fallback: check mariadb
        pkg_check_modules(MARIADB libmariadb QUIET)
        if(MARIADB_FOUND)
            message(STATUS "MariaDB (pkg-config) found. Enabling MySQL bindings.")
            add_compile_definitions(USE_MYSQL)
            include_directories(${MARIADB_INCLUDE_DIRS})
            link_directories(${MARIADB_LIBRARY_DIRS})
            list(APPEND COMMON_LIBS ${MARIADB_LIBRARIES})
        else()
            message(WARNING "MySQL/MariaDB NOT found. mysqlConnect, mysqlQuery, etc. will not be available.")
        endif()
    endif()
endif()

# 2. Ativa o suporte a OpenSSL na biblioteca httplib
target_compile_definitions(sapphire_core PUBLIC
    SFML_STATIC
    CPPHTTPLIB_OPENSSL_SUPPORT
)

target_include_directories(sapphire_core PUBLIC
    src/
    src/parser/
    src/compiler/
    ${VM_INCLUDE_DIR}
    src/api/
    src/graphics/
    src/utils/
    src/preprocessor/
    ${httplib_SOURCE_DIR}
    ${asmjit_SOURCE_DIR}
)

# 3. Linka o OpenSSL à biblioteca principal
target_link_libraries(sapphire_core PUBLIC
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

target_link_libraries(sapphire_core PUBLIC OpenCL sqlite3 asmjit)

add_executable(sapphire src/main.cpp)

# Runner executável (usa a mesma VM que o sapphire.exe)
add_executable(runner src/vm/runner.cpp src/topaz/third_party/miniz.c)

add_executable(beryl src/beryl/main.cpp src/beryl/beryl.cpp src/beryl/beryl_ui.cpp src/topaz/third_party/miniz.c)
add_executable(citrine src/linter/main.cpp src/linter/citrine.cpp)
target_link_libraries(citrine PUBLIC sapphire_core)

add_executable(garnet src/test_runner/main.cpp src/test_runner/garnet.cpp)
target_link_libraries(garnet PUBLIC sapphire_core)

add_executable(amethyst src/formatter/main.cpp src/formatter/amethyst.cpp)
target_link_libraries(amethyst PUBLIC sapphire_core)

add_executable(quartz src/benchmarker/main.cpp src/benchmarker/quartz.cpp)
target_link_libraries(quartz PUBLIC sapphire_core)


# Build new modular topaz (v2) - full-featured package manager
option(BUILD_TOPAZ "Build new modular topaz package manager" ON)
if(BUILD_TOPAZ)
    add_subdirectory(src/topaz)
else()
    # Only build old topaz if new topaz is not being built
    add_executable(topaz src/topaz/topaz.cpp)
endif()

target_compile_definitions(sapphire PRIVATE SFML_STATIC)
target_compile_definitions(runner PRIVATE SFML_STATIC)
target_compile_definitions(beryl PRIVATE SFML_STATIC)

set(COMMON_LIBS
    sapphire_core
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenSSL::SSL
    OpenSSL::Crypto
)

list(APPEND COMMON_LIBS OpenCL sqlite3)

# Topaz doesn't need OpenCL, SQLite, or SFML - only OpenSSL for HTTPS
set(TOPAZ_LIBS
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    list(APPEND COMMON_LIBS
        ws2_32
        crypt32
        psapi
        user32
        gdi32
        bcrypt
        opengl32
        winmm
    )
    if(NOT MSVC)
        list(APPEND COMMON_LIBS -pthread)
    endif()
elseif(APPLE)
    # macOS specific libraries
    list(APPEND COMMON_LIBS
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
    )
    find_library(PTHREAD_LIBRARY pthread)
    if(PTHREAD_LIBRARY)
        list(APPEND COMMON_LIBS ${PTHREAD_LIBRARY})
    else()
        list(APPEND COMMON_LIBS -pthread)
    endif()
elseif(UNIX)
    # Linux specific libraries
    list(APPEND COMMON_LIBS
        pthread
        dl
        GL
        X11
        Xcursor
        Xrandr
        Xi
    )
endif()

target_link_libraries(sapphire PRIVATE ${COMMON_LIBS})
target_link_libraries(runner PRIVATE ${COMMON_LIBS})
target_link_libraries(beryl PRIVATE ${COMMON_LIBS})

# Topaz only needs OpenSSL for HTTPS, not OpenCL/SQLite/SFML
if(BUILD_TOPAZ)
    target_link_libraries(topaz PRIVATE ${TOPAZ_LIBS})
else()
    target_link_libraries(topaz PRIVATE ${COMMON_LIBS})
endif()

# Optional: Build standalone topaz (without Sapphire dependencies)
option(BUILD_TOPAZ_STANDALONE "Build topaz as standalone executable (no Sapphire dependencies)" OFF)
if(BUILD_TOPAZ_STANDALONE)
    add_executable(topaz_standalone
        src/topaz/topaz.cpp
        src/utils/termcolor.cpp
    )
    target_compile_definitions(topaz_standalone PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_include_directories(topaz_standalone PRIVATE
        src/
        src/utils/
        ${LIBS_DIR}
        ${OPENSSL_INCLUDE_DIR}
    )
    target_link_libraries(topaz_standalone PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    if(WIN32)
        target_link_libraries(topaz_standalone PRIVATE
            ws2_32
            crypt32
        )
        if(NOT MSVC)
            target_link_libraries(topaz_standalone PRIVATE -pthread)
        endif()
    elseif(APPLE)
        target_link_libraries(topaz_standalone PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
        )
        find_library(PTHREAD_LIBRARY pthread)
        if(PTHREAD_LIBRARY)
            target_link_libraries(topaz_standalone PRIVATE ${PTHREAD_LIBRARY})
        else()
            target_link_libraries(topaz_standalone PRIVATE -pthread)
        endif()
    elseif(UNIX)
        target_link_libraries(topaz_standalone PRIVATE pthread dl)
    endif()
endif()

if(WIN32)
#    add_custom_command(TARGET sapphire POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:OpenCL>
#        $<TARGET_FILE_DIR:sapphire>
#    )
#    add_custom_command(TARGET runner POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:OpenCL>
#        $<TARGET_FILE_DIR:runner>
#    )
#    add_custom_command(TARGET mine POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:OpenCL>
#        $<TARGET_FILE_DIR:mine>
#    )
#    add_custom_command(TARGET beryl POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:OpenCL>
#        $<TARGET_FILE_DIR:beryl>
#    )
endif()