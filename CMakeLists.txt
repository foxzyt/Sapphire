# Versão mínima do CMake
cmake_minimum_required(VERSION 3.25)

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

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.29.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
FetchContent_MakeAvailable(httplib)

# Define o padrão do C++ para C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define o nome do executável final
set(EXECUTABLE_NAME sapphire)

# Adiciona uma definição para ativar o código de depuração
add_compile_definitions(DEBUG_PRINT_CODE)

# Lista todos os arquivos-fonte (.cpp) que compõem o projeto
set(SOURCES
        # keep-sorted start
        src/bytecode_io.cpp
        src/compiler.cpp
        src/lexer.cpp
        src/main.cpp
        src/object.cpp
        src/parser.cpp
        src/sfml_graphics_natives.cpp
        src/utils.cpp
        src/value.cpp
        src/vm.cpp
        # keep-sorted end
        src/debug.cpp # <<< ADICIONE ESTA LINHA
)

# Lista os diretórios onde os arquivos de cabeçalho (.h) estão
include_directories(src)

# Cria o executável a partir dos arquivos-fonte
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_features(sapphire PRIVATE cxx_std_20)
target_compile_options(sapphire PRIVATE -g)

# Link 
target_link_libraries(sapphire PRIVATE SFML::Graphics)
target_link_libraries(sapphire PRIVATE httplib)
target_link_libraries(sapphire PRIVATE nlohmann_json::nlohmann_json)

