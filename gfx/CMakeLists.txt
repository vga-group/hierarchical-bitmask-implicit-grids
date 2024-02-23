cmake_minimum_required(VERSION 3.20)

project(RaybaseGfx LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT RAYBASE_CORE_INCLUDE OR NOT RAYBASE_CORE_LIBRARY)
    message(FATAL_ERROR "You must define RAYBASE_CORE_INCLUDE and RAYBASE_CORE_LIBRARY")
endif()

option(FORCE_VENDOR_ALL "Always use vendored versions of libraries" ON)
option(USE_SHADER_REGISTRY "Store shader metadata (used for hot reloading)" ON)
if(NOT FORCE_VENDOR_ALL)
    find_package(Vulkan)
endif()

if (NOT Vulkan_FOUND)
    add_subdirectory(external/Vulkan-Headers)
    add_subdirectory(external/Vulkan-Loader)
endif()

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()
#set(VOLK_STATIC_DEFINES ${VOLK_STATIC_DEFINES} VOLK_VULKAN_H_PATH="../Vulkan-Headers/include/vulkan/vulkan.h")
set(VOLK_PULL_IN_VULKAN OFF)
add_subdirectory(external/volk)

add_subdirectory(external/fuchsia_radix_sort)
# Stupid patchwork to make volk work with this library :/
if(MSVC)
    target_compile_options(vk-radix-sort PRIVATE /FI"volk.h")
else()
    target_compile_options(vk-radix-sort PRIVATE -include volk.h)
endif()
target_link_libraries(vk-radix-sort PRIVATE volk)
target_compile_definitions(vk-radix-sort PUBLIC VK_NO_PROTOTYPES)

function(add_shader target)
    if(NOT TARGET shaders)
        # This custom target is used for the shader hot-reloading functionality.
        add_custom_target(shaders)
    endif()
    foreach(source ${ARGN})
        get_filename_component(shader_bin_name ${source} NAME)
        string(REPLACE "\." "_" shader_bin_name ${shader_bin_name})
        string(CONCAT header ${source} ".h")

        list(TRANSFORM SHADER_INCLUDE_DIRS PREPEND "-I" OUTPUT_VARIABLE SHADER_INCLUDE_ARGS)

        if(USE_SHADER_REGISTRY)
            set(registry_string "RB_SHADER_REGISTRY_ENTRY(${shader_bin_name}_shader_binary, \"${CMAKE_CURRENT_BINARY_DIR}/${header}\", \"${CMAKE_CURRENT_SOURCE_DIR}/${source}\")")
        else()
            set(registry_string "")
        endif()
        set(preamble_string "static const uint32_t ${shader_bin_name}_shader_binary[] = ")

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${header}
            COMMAND glslc --target-env=vulkan1.2 -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/${source} -o ${CMAKE_CURRENT_BINARY_DIR}/${header} ${SHADER_INCLUDE_ARGS} -O -g
            COMMAND ${CMAKE_COMMAND} -DPREPEND_CONTENT=${preamble_string} -DAPPEND_FILE=${CMAKE_CURRENT_BINARY_DIR}/${header} -DAPPEND_CONTENT=${registry_string} -P ${RAYBASE_ROOT}/cmake/append.cmake
            COMMAND_EXPAND_LISTS
            MAIN_DEPENDENCY ${source}
            DEPENDS ${source}
            IMPLICIT_DEPENDS CXX ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            VERBATIM
        )
        target_sources(shaders PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${header})
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${header})
    endforeach()
    add_dependencies(${target} shaders)
endfunction()

set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STRIPPER OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/SPIRV-Reflect)

find_package(ICU COMPONENTS uc REQUIRED)

set(raybase-gfx-sources
    external/stb_image.cc
    external/vk_mem_alloc.cc
    acceleration_structure.cc
    blit_stage.cc
    camera.cc
    clear_stage.cc
    clustering_stage.cc
    compute_pipeline.cc
    context.cc
    descriptor_set.cc
    device.cc
    environment_map.cc
    envmap_stage.cc
    event.cc
    forward_stage.cc
    framebuffer.cc
    garbage_collector.cc
    gbuffer.cc
    gpu_buffer.cc
    gpu_pipeline.cc
    headless.cc
    light.cc
    material.cc
    mesh.cc
    model.cc
    multiview_forward_stage.cc
    multiview_layout_stage.cc
    path_tracer.cc
    path_tracer_stage.cc
    primitive.cc
    radix_sort.cc
    raster_pipeline.cc
    ray_tracing_pipeline.cc
    render_pass.cc
    render_pipeline.cc
    render_stage.cc
    render_target.cc
    sampler.cc
    scene_stage.cc
    texture.cc
    timer.cc
    tonemap_stage.cc
    video_output.cc
    vkres.cc
    vulkan_helpers.cc
)
if(RAYBASE_ENABLE_SDL)
    set(raybase-gfx-sources ${raybase-gfx-sources} window.cc)
endif()

add_library(raybase-gfx STATIC ${raybase-gfx-sources})

add_shader(raybase-gfx
    alias_table_importance.comp
    clustering.comp
    clustering_hierarchy.comp
    decal_order.comp
    decal_ranges.comp
    envmap.frag
    envmap.vert
    equirectangular_to_cubemap_convert.comp
    forward.vert
    forward.frag
    animation.comp
    light_morton.comp
    light_ranges.comp
    multiview_forward.frag
    multiview_forward.vert
    multiview_z_pre_pass.vert
    multiview_layout_grid.comp
    multiview_layout_blend.comp
    path_tracer.rgen
    point_light_tlas_instance.comp
    rt_generic.rchit
    rt_generic.rmiss
    rt_generic.rahit
    rt_generic_point_light.rchit
    rt_generic_point_light.rahit
    rt_generic_point_light.rint
    sort_order.comp
    sort_placement.comp
    tonemap.comp
    tonemap_resolve.comp
    tonemap_msaa.comp
    tri_light_extract.comp
    z_pre_pass.vert
    z_pre_pass.frag
)

target_link_libraries(raybase-gfx PUBLIC ICU::uc ${RAYBASE_CORE_LIBRARY})
target_link_libraries(raybase-gfx PRIVATE volk spirv-reflect-static)
target_link_libraries(raybase-gfx PUBLIC vk-radix-sort)
target_compile_definitions(raybase-gfx PUBLIC VK_NO_PROTOTYPES)

if (Vulkan_FOUND)
    target_include_directories(raybase-gfx PUBLIC "external/Vulkan-Headers/include")
    if (NOT WIN32)
        target_link_libraries(raybase-gfx PUBLIC Vulkan::Vulkan)
    endif()
else()
    target_include_directories(raybase-gfx PUBLIC "external/Vulkan-Headers/include")
    if (NOT WIN32)
        target_link_libraries(raybase-gfx PUBLIC vulkan)
    endif()
endif()

set_property(TARGET raybase-gfx PROPERTY CXX_STANDARD 17)
set_property(TARGET raybase-gfx PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET raybase-gfx PROPERTY CXX_EXTENSIONS OFF)

set(RAYBASE_GFX_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/VulkanMemoryAllocator/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/volk"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/KTX-Software/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Reflect"
)
set(RAYBASE_GFX_INCLUDE ${RAYBASE_GFX_INCLUDE} PARENT_SCOPE)
set(RAYBASE_GFX_LIBRARY raybase-gfx PARENT_SCOPE)
target_include_directories(raybase-gfx PUBLIC ${RAYBASE_GFX_INCLUDE} ${RAYBASE_CORE_INCLUDE} ${CMAKE_CURRENT_BINARY_DIR})
