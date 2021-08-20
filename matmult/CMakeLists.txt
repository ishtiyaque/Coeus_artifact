
cmake_minimum_required(VERSION 3.10)

project(Matmult VERSION 1.0 LANGUAGES CXX)

# [Option] CXX compiler flags
# For easier adding of CXX compiler flags
include(CheckCXXCompilerFlag)
function(seal_enable_cxx_compiler_flag_if_supported flag)
    string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)
    if(flag_already_set EQUAL -1)
        message(STATUS "Adding CXX compiler flag: ${flag} ...")
        check_cxx_compiler_flag("${flag}" flag_supported)
        if(flag_supported)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
        endif()
        unset(flag_supported CACHE)
    endif()
endfunction()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)

seal_enable_cxx_compiler_flag_if_supported("-O3")


# Executable will be in /bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)


add_executable(baseline baseline.cpp)
add_executable(opt1 opt1.cpp)
add_executable(opt2 opt2.cpp)

# Import Microsoft SEAL
find_package(SEAL 3.5  REQUIRED)

# Link Microsoft SEAL
target_link_libraries(baseline SEAL::seal)
target_link_libraries(opt1 SEAL::seal)
target_link_libraries(opt2 SEAL::seal)
