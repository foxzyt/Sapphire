# Versão mínima do CMake
cmake_minimum_required(VERSION 3.10)

# Nome do projeto
project(Sapphire)

# Define o padrão do C++ para C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define o nome do executável final
set(EXECUTABLE_NAME sapphire)

# Adiciona uma definição para ativar o código de depuração
add_compile_definitions(DEBUG_PRINT_CODE)

# Lista todos os arquivos-fonte (.cpp) que compõem o projeto
set(SOURCES
    src/main.cpp
    src/lexer.cpp
    src/compiler.cpp
    src/parser.cpp
    src/object.cpp
    src/vm.cpp
    src/value.cpp
    src/debug.cpp # <<< ADICIONE ESTA LINHA
)

# Lista os diretórios onde os arquivos de cabeçalho (.h) estão
include_directories(src)

# Cria o executável a partir dos arquivos-fonte
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_options(sapphire PRIVATE -g)