add_library(avm_gpu STATIC
    vulkan_renderer.cpp
    opengl_renderer.cpp
    gfxstream_bridge.cpp
)

target_include_directories(avm_gpu PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(AVM_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    target_link_libraries(avm_gpu PUBLIC Vulkan::Vulkan)
    target_compile_definitions(avm_gpu PRIVATE AVM_VULKAN_ENABLED)
endif()
