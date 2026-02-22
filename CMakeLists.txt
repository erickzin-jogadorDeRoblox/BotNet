# CMakeLists.txt

cmake_minimum_required(VERSION 3.14...3.28)

# Nome do projeto
project(C2_Project
    VERSION 0.1
    DESCRIPTION "Servidor C2 + controlador + bot simples"
    LANGUAGES CXX
)

# Usa C++17 (ou 20 se preferir)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++17 ao invés de -std=gnu++17

# Onde estão os arquivos fonte
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

# Coleta todos os .cpp da pasta src/
file(GLOB SOURCES "${SOURCE_DIR}/*.cpp")


# ───────────────────────────────────────────────
# Executáveis separados
# ───────────────────────────────────────────────

add_executable(server   src/server.cpp)
add_executable(controller src/controller.cpp)
add_executable(infected  src/infected.cpp)

target_include_directories(server    PRIVATE ${SOURCE_DIR})
target_include_directories(controller PRIVATE ${SOURCE_DIR})
target_include_directories(infected  PRIVATE ${SOURCE_DIR})

target_link_libraries(server    PRIVATE pthread)
target_link_libraries(controller PRIVATE pthread)
target_link_libraries(infected  PRIVATE pthread)

# ───────────────────────────────────────────────
# Dicas extras (descomente se precisar)
# ───────────────────────────────────────────────

# Ativa símbolos de debug em build Debug
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Define uma constante no código (ex: versão)
# target_compile_definitions(server PRIVATE
#     PROJECT_VERSION="${PROJECT_VERSION}"
# )
# target_compile_definitions(controller PRIVATE
#     PROJECT_VERSION="${PROJECT_VERSION}"
# )
# target_compile_definitions(infected PRIVATE
#     PROJECT_VERSION="${PROJECT_VERSION}"
# )

# Se quiser instalar os binários
# install(TARGETS server controller infected DESTINATION bin)