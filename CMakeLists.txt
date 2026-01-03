cmake_minimum_required(VERSION 3.25)
project(sapphire CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SFML_STATIC_LIBRARIES TRUE)

if(MSVC)
    add_compile_options(/O2 /MT /DNDEBUG)
    set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MSVC/lib/cmake/SFML")
    set(Freetype_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MSVC/lib/cmake/freetype")
    find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)
else()
    add_compile_options(-O3 -march=native -DNDEBUG)
    set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MinGW/lib/cmake/SFML")
    set(Freetype_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MinGW/lib/cmake/freetype")
    find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
endif()

add_library(sapphire_core STATIC
    src/compiler.cpp
    src/debug.cpp
    src/lexer.cpp
    src/object.cpp
    src/parser.cpp
    src/value.cpp
    src/vm.cpp
    src/utils.cpp
    src/sfml_graphics_natives.cpp
    src/bytecode_io.cpp
    src/sapphire_api.cpp
)

target_compile_definitions(sapphire_core PUBLIC
    SFML_STATIC
    CPPHTTPLIB_OPENSSL_SUPPORT
)

if(MSVC)
    target_include_directories(sapphire_core PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MSVC/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/include"
    )
else()
    target_include_directories(sapphire_core PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SFML-3.0.2-MinGW/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/include"
    )
endif()

add_executable(sapphire src/main.cpp)

if(MSVC)
    target_link_libraries(sapphire PRIVATE
        sapphire_core
        SFML::Graphics
        SFML::Window
        SFML::System
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/lib-msvc/libssl.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/lib-msvc/libcrypto.lib"
        ws2_32
        crypt32
        psapi
        user32
        gdi32
        bcrypt
        opengl32
        winmm
    )
else()
    target_link_libraries(sapphire PRIVATE
        sapphire_core
        SFML::Graphics
        SFML::Window
        SFML::System
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/lib-mingw/libssl.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openssl/lib-mingw/libcrypto.a"
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