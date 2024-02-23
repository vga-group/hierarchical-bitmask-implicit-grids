cmake_minimum_required(VERSION 3.20)

project(Raybase LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options(-fcolor-diagnostics)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions("RAYBASE_DEBUG")
endif()

if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    add_compile_definitions("RAYBASE_BIG_ENDIAN")
endif()

set(RAYBASE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
string(LENGTH "${RAYBASE_ROOT}/" RAYBASE_ROOT_PATH_SIZE)
add_compile_definitions("RAYBASE_ROOT_PATH_SIZE=${RAYBASE_ROOT_PATH_SIZE}")

add_subdirectory(core)
add_subdirectory(gfx)
add_subdirectory(phys)
add_subdirectory(extra)

add_executable(ig_benchmark ig_benchmark.cc)
target_link_libraries(ig_benchmark PUBLIC raybase-core raybase-gfx raybase-phys raybase-extra)
set_property(TARGET ig_benchmark PROPERTY C_STANDARD 11)
set_property(TARGET ig_benchmark PROPERTY CXX_STANDARD 17)
set_property(TARGET ig_benchmark PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ig_benchmark PROPERTY CXX_EXTENSIONS OFF)
