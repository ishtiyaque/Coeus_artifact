cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(SealPIR VERSION 2.1 LANGUAGES CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories( ${CMAKE_SOURCE_DIR}/../common)

add_executable(client client.cpp)

add_library(sealpir STATIC
  pir.cpp
  pir_client.cpp
)

find_package(SEAL 3.2.0 EXACT REQUIRED)

target_link_libraries(client sealpir SEAL::seal rpc)
