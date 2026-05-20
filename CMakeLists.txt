cmake_minimum_required(VERSION 3.12)
project(ParallelWFC LANGUAGES CXX)

# Force C++17 standard (or use 20/23 if preferred)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
    add_compile_definitions(_DEBUG)
endif()

# Find the MPI package (specifically looking for the CXX components)
find_package(MPI REQUIRED COMPONENTS CXX)
include_directories("include")
# Define your executable and its source file(s)
file(GLOB_RECURSE PWFC_SOURCES "src/*.cpp")
add_executable(mpipwfc ${PWFC_SOURCES})

# Link the MPI::MPI_CXX target to your executable
# This automatically handles include directories and compiler flags
target_link_libraries(mpipwfc PRIVATE MPI::MPI_CXX)
