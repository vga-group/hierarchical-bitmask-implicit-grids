cmake_minimum_required(VERSION 3.19)

project(RaybaseCore LANGUAGES C CXX)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Threads)

option(RAYBASE_ENABLE_SDL "Include SDL2 support" ON)

if(RAYBASE_ENABLE_SDL)
    find_package(SDL2 REQUIRED)
endif()

find_package(glm REQUIRED)

set(raybase-core-sources
    animation.cc
    bitset.cc
    cvar.cc
    filesystem.cc
    io.cc
    log.cc
    math.cc
    resource_store.cc
    stack_allocator.cc
    skeleton.cc
    string.cc
    thread_pool.cc
    transformable.cc
    translator.cc
)
if(RAYBASE_ENABLE_SDL)
    set(raybase-core-sources ${raybase-core-sources} sdl.cc)
endif()

add_library(raybase-core STATIC ${raybase-core-sources})

set(RAYBASE_CORE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/external" ${RAYBASE_ROOT})
set(RAYBASE_CORE_INCLUDE ${RAYBASE_CORE_INCLUDE} PARENT_SCOPE)
set(RAYBASE_CORE_LIBRARY raybase-core PARENT_SCOPE)
target_include_directories(raybase-core PUBLIC ${RAYBASE_CORE_INCLUDE})

target_link_libraries(raybase-core PUBLIC Threads::Threads glm::glm)

if(RAYBASE_ENABLE_SDL)
    target_link_libraries(raybase-core PUBLIC SDL2::SDL2)
    target_compile_definitions(raybase-core PUBLIC RAYBASE_HAS_SDL2)
endif()

if(MSVC)
    target_compile_options(raybase-core PUBLIC /arch:SSE4.1)
else()
    target_compile_options(raybase-core PUBLIC -msse4.1)
endif()

set_property(TARGET raybase-core PROPERTY CXX_STANDARD 17)
set_property(TARGET raybase-core PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET raybase-core PROPERTY CXX_EXTENSIONS OFF)
