# Versão mínima do CMake
cmake_minimum_required(VERSION 3.28)

# Nome do projeto
project(Sapphire LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.29.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
FetchContent_MakeAvailable(httplib)

# Define o padrão do C++ para C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define o nome do executável final
set(EXECUTABLE_NAME sapphire)

# Adiciona uma definição para ativar o código de depuração
add_compile_definitions(DEBUG_PRINT_CODE)

# Lista todxos os arquivos-fonte (.cpp) que compõem o projeto
FILE(GLOB SOURCES src/*.cpp)
set(SOURCES
        # keep-sorted start
        src/main.cpp
        src/utils.cpp
        src/bytecode_io.cpp
        src/lexer.cpp
        src/compiler.cpp
        src/parser.cpp
        src/sfml_graphics_natives.cpp
        src/object.cpp
        src/vm.cpp
        src/value.cpp
        src/debug.cpp # <<< ADICIONE ESTA LINHA
        # keep-sorted end
)

# Lista os diretórios onde os arquivos de cabeçalho (.h) estão
include_directories(src)

# Cria o executável a partir dos arquivos-fonte
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_features(sapphire PRIVATE cxx_std_17)
target_compile_options(sapphire PRIVATE -g)
target_link_libraries(sapphire PRIVATE SFML::Graphics)
target_link_libraries(sapphire PRIVATE httplib)