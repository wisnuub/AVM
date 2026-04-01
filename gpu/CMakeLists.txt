add_library(avm_gpu STATIC
    vulkan_renderer.cpp
    opengl_renderer.cpp
    gfxstream_bridge.cpp
    frame_presenter.cpp
)

target_include_directories(avm_gpu PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# --- Vulkan ---
if(AVM_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    target_link_libraries(avm_gpu PUBLIC Vulkan::Vulkan)
    target_compile_definitions(avm_gpu PRIVATE AVM_VULKAN_ENABLED)
endif()

# --- SDL2 (windowing + OpenGL context) ---
find_package(SDL2 REQUIRED)
target_link_libraries(avm_gpu PUBLIC SDL2::SDL2)

# --- OpenGL (fallback renderer) ---
if(AVM_PLATFORM_LINUX OR AVM_PLATFORM_MACOS)
    find_package(OpenGL REQUIRED)
    target_link_libraries(avm_gpu PUBLIC OpenGL::GL)
elseif(AVM_PLATFORM_WINDOWS)
    target_link_libraries(avm_gpu PUBLIC opengl32)
endif()

message(STATUS "GPU module: Vulkan=${AVM_ENABLE_VULKAN}, ANGLE=${AVM_ENABLE_ANGLE}")
