cmake_minimum_required(VERSION 3.20)
project(AVM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(AVM_ENABLE_KVM     "Enable KVM hypervisor backend (Linux)"   ON)
option(AVM_ENABLE_WHPX    "Enable WHPX hypervisor backend (Windows)" ON)
option(AVM_ENABLE_VULKAN  "Enable Vulkan host renderer"              ON)
option(AVM_ENABLE_ANGLE   "Enable ANGLE OpenGL->Vulkan translation"  ON)
option(AVM_ENABLE_OVERLAY "Build the ImGui overlay UI"               ON)

# Platform detection
if(UNIX AND NOT APPLE)
    set(AVM_PLATFORM_LINUX TRUE)
elseif(WIN32)
    set(AVM_PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(AVM_PLATFORM_MACOS TRUE)
endif()

# Global includes
include_directories(${CMAKE_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(core)
add_subdirectory(gpu)
add_subdirectory(input)
add_subdirectory(overlay)

# Main AVM executable
add_executable(avm
    src/main.cpp
)

target_link_libraries(avm
    avm_core
    avm_gpu
    avm_input
    avm_overlay
)

message(STATUS "AVM v${PROJECT_VERSION} — Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "KVM: ${AVM_ENABLE_KVM} | WHPX: ${AVM_ENABLE_WHPX} | Vulkan: ${AVM_ENABLE_VULKAN}")
