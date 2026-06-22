cmake_minimum_required(VERSION 3.10)
project(sapphire CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /MT /DNDEBUG)
else()
    add_compile_options(-O3 -march=native -DNDEBUG)
endif()

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
endif()

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(SFML_STATIC_LIBRARIES ON)
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2/lib/cmake/SFML")
find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/libs"
    ${OPENSSL_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/include"
)

# 3. Linka o OpenSSL à biblioteca principal
target_link_libraries(sapphire_core PUBLIC
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenSSL::SSL
    OpenSSL::Crypto
)

add_executable(sapphire src/main.cpp)
add_executable(runner src/vm/runner.cpp)
add_executable(mine src/mine/mine.cpp)
add_executable(spack src/spack/main.cpp src/spack/spack.cpp src/spack/spack_ui.cpp)

target_compile_definitions(sapphire PRIVATE SFML_STATIC)
target_compile_definitions(runner PRIVATE SFML_STATIC)
target_compile_definitions(mine PRIVATE SFML_STATIC)
target_compile_definitions(spack PRIVATE SFML_STATIC)

set(COMMON_LIBS
    sapphire_core
    SFML::Graphics
    SFML::Window
    SFML::System
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
        -pthread
    )
endif()

target_link_libraries(sapphire PRIVATE ${COMMON_LIBS})
target_link_libraries(runner PRIVATE ${COMMON_LIBS})
target_link_libraries(mine PRIVATE ${COMMON_LIBS})
target_link_libraries(spack PRIVATE ${COMMON_LIBS})