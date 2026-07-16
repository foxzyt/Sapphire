cmake_minimum_required(VERSION 3.11)
project(mine CXX)

include(FetchContent)

# Check if third_party libraries exist locally
set(USE_LOCAL_LIBS TRUE)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
    set(USE_LOCAL_LIBS FALSE)
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json.hpp")
    set(USE_LOCAL_LIBS FALSE)
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz.c")
    set(USE_LOCAL_LIBS FALSE)
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/termcolor.hpp")
    set(USE_LOCAL_LIBS FALSE)
endif()

if(NOT USE_LOCAL_LIBS)
    message(STATUS "Third-party libraries not found locally, downloading...")
    
    # Fetch httplib
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.15.3
    )
    FetchContent_MakeAvailable(httplib)
    
    # Fetch nlohmann/json
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
    
    # Fetch miniz (use single file directly instead of CMake project)
    FetchContent_Declare(
        miniz
        URL https://raw.githubusercontent.com/richgel999/miniz/master/miniz.c
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(miniz)
    
    # Fetch miniz header
    FetchContent_Declare(
        miniz_header
        URL https://raw.githubusercontent.com/richgel999/miniz/master/miniz.h
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(miniz_header)
    
    # Fetch termcolor
    FetchContent_Declare(
        termcolor
        GIT_REPOSITORY https://github.com/ikalnitsky/termcolor.git
        GIT_TAG        v2.0.0
    )
    FetchContent_MakeAvailable(termcolor)
else()
    message(STATUS "Using local third-party libraries")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /MT /DNDEBUG)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
endif()

if(WIN32 AND CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Find OpenSSL
if(NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/openssl")
endif()

if(NOT DEFINED OPENSSL_USE_STATIC_LIBS)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

find_package(OpenSSL REQUIRED)

# Define httplib with OpenSSL support
add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)

# Set library paths based on local or remote
if(USE_LOCAL_LIBS)
    set(MINIZ_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz.c")
    set(HTTPLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    set(MINIZ_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    set(TERMCOLOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
else()
    set(MINIZ_SOURCE "${miniz_SOURCE_DIR}")
    set(HTTPLIB_INCLUDE_DIR "${httplib_SOURCE_DIR}")
    set(JSON_INCLUDE_DIR "${nlohmann_json_SOURCE_DIR}/include")
    set(MINIZ_INCLUDE_DIR "${miniz_header_SOURCE_DIR}")
    set(TERMCOLOR_INCLUDE_DIR "${termcolor_SOURCE_DIR}")
endif()

# Create executable
add_executable(mine
    src/main.cpp
    ${MINIZ_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils/termcolor.cpp
)

# Set output directory to be in the main build directory
set_target_properties(mine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include directories
target_include_directories(mine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HTTPLIB_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR}
    ${MINIZ_INCLUDE_DIR}
    ${TERMCOLOR_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils
    ${OPENSSL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(mine PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(mine PRIVATE
        ws2_32
        crypt32
    )
    if(NOT MSVC)
        target_link_libraries(mine PRIVATE -pthread)
    endif()
endif()
