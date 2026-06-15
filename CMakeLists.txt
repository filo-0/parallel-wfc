cmake_minimum_required(VERSION 3.12)
project(ParallelWFC LANGUAGES CXX)

# Force C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type set to ${CMAKE_BUILD_TYPE}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "CLANG")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -Wall -Wextra -Wpedantic)
        add_compile_definitions(_DEBUG)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/Ox)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Zi /Wall)
        add_compile_definitions(_DEBUG)
    endif()
endif()

set(STB_IMAGE_SHARED ON)
add_subdirectory("vendor/stb-image")

# Find the MPI package (specifically looking for the CXX components)
find_package(MPI REQUIRED COMPONENTS CXX)
include_directories("include")

add_executable(mpipwfc "src/generation.cpp" "src/mpipwfc.cpp")
add_executable(swfc "src/generation.cpp" "src/swfc.cpp")

# Link the MPI::MPI_CXX target to your executable
# This automatically handles include directories and compiler flags
target_link_libraries(mpipwfc PRIVATE MPI::MPI_CXX stb-image)
target_link_libraries(swfc PRIVATE stb-image)
