# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

cmake_minimum_required(VERSION 3.10)

project(Coeus VERSION 1.0 LANGUAGES CXX)

# Executable will be in ./bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories( ${CMAKE_SOURCE_DIR}/../../common)


# Import Microsoft SEAL
find_package(SEAL 3.5 REQUIRED)


add_executable(split split.cpp )
add_executable(merge merge.cpp )

# Link Microsoft SEAL
target_link_libraries(split SEAL::seal rpc)
target_link_libraries(merge SEAL::seal rpc)