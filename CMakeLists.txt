cmake_minimum_required(VERSION 3.11)
project(sapphire C CXX)

if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
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

# Fetch Sapphire Libs (OpenSSL, SFML, httplib, nlohmann, freetype)
FetchContent_Declare(
    sapphire_libs
    URL https://github.com/foxzyt/Sapphire/releases/download/v1.0.7/libs.zip
)
FetchContent_MakeAvailable(sapphire_libs)

# Define LIBS_DIR
set(LIBS_DIR "${sapphire_libs_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "${LIBS_DIR}/openssl")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /MT /DNDEBUG)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

if(NOT DEFINED OPENSSL_USE_STATIC_LIBS)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)

if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
endif()

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

if(NOT DEFINED SFML_STATIC_LIBRARIES)
    set(SFML_STATIC_LIBRARIES ON)
endif()
if(NOT SFML_DIR)
    set(SFML_DIR "${LIBS_DIR}/SFML-3.0.2/lib/cmake/SFML")
endif()
find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_compile_definitions(HAS_OPENCL)

add_library(sapphire_core STATIC
    src/compiler/compiler.cpp
    src/compiler/debug.cpp
    src/parser/lexer.cpp
    src/vm/object.cpp
    src/parser/parser.cpp
    src/vm/value.cpp
    src/vm/vm.cpp
    src/utils/utils.cpp
    src/utils/termcolor.cpp
    src/graphics/engine.cpp
    src/graphics/vec2d.cpp
    src/graphics/vec3d.cpp
    src/compiler/bytecode_io.cpp
    src/api/sapphire_api.cpp
    src/api/opencl_api.cpp
    src/api/sqlite_api.cpp
    src/preprocessor/preprocessor.cpp
)

# 2. Ativa o suporte a OpenSSL na biblioteca httplib
target_compile_definitions(sapphire_core PUBLIC
    SFML_STATIC
    CPPHTTPLIB_OPENSSL_SUPPORT
)

target_include_directories(sapphire_core PUBLIC
    src/
    src/parser/
    src/compiler/
    src/vm/
    src/api/
    src/graphics/
    src/utils/
    src/preprocessor/
    "${LIBS_DIR}"
    ${OPENSSL_INCLUDE_DIR}
)

# 3. Linka o OpenSSL à biblioteca principal
target_link_libraries(sapphire_core PUBLIC
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(sapphire_core PUBLIC OpenCL sqlite3)

add_executable(sapphire src/main.cpp)
add_executable(runner src/vm/runner.cpp)
add_executable(spack src/spack/main.cpp src/spack/spack.cpp src/spack/spack_ui.cpp)

# Build new modular spark (v2) - full-featured package manager
option(BUILD_SPARK "Build new modular spark package manager" ON)
if(BUILD_SPARK)
    add_subdirectory(src/spark)
else()
    # Only build old spark if new spark is not being built
    add_executable(spark src/spark/spark.cpp)
endif()

target_compile_definitions(sapphire PRIVATE SFML_STATIC)
target_compile_definitions(runner PRIVATE SFML_STATIC)
target_compile_definitions(spack PRIVATE SFML_STATIC)

set(COMMON_LIBS
    sapphire_core
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenSSL::SSL
    OpenSSL::Crypto
)

list(APPEND COMMON_LIBS OpenCL sqlite3)

# Spark doesn't need OpenCL, SQLite, or SFML - only OpenSSL for HTTPS
set(SPARK_LIBS
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
endif()

target_link_libraries(sapphire PRIVATE ${COMMON_LIBS})
target_link_libraries(runner PRIVATE ${COMMON_LIBS})
target_link_libraries(spack PRIVATE ${COMMON_LIBS})

# Spark only needs OpenSSL for HTTPS, not OpenCL/SQLite/SFML
if(BUILD_SPARK)
    target_link_libraries(spark PRIVATE ${SPARK_LIBS})
else()
    target_link_libraries(spark PRIVATE ${COMMON_LIBS})
endif()

# Optional: Build standalone spark (without Sapphire dependencies)
option(BUILD_SPARK_STANDALONE "Build spark as standalone executable (no Sapphire dependencies)" OFF)
if(BUILD_SPARK_STANDALONE)
    add_executable(spark_standalone
        src/spark/spark.cpp
        src/utils/termcolor.cpp
    )
    target_compile_definitions(spark_standalone PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_include_directories(spark_standalone PRIVATE
        src/
        src/utils/
        ${LIBS_DIR}
        ${OPENSSL_INCLUDE_DIR}
    )
    target_link_libraries(spark_standalone PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    if(WIN32)
        target_link_libraries(spark_standalone PRIVATE
            ws2_32
            crypt32
        )
        if(NOT MSVC)
            target_link_libraries(spark_standalone PRIVATE -pthread)
        endif()
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
#    add_custom_command(TARGET spack POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:OpenCL>
#        $<TARGET_FILE_DIR:spack>
#    )
endif()