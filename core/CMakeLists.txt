set(AVM_CORE_SOURCES
    vm_manager.cpp
    hypervisor/hypervisor_detect.cpp
)

# macOS: Hypervisor.framework backend (Objective-C++ .mm file)
if(AVM_PLATFORM_MACOS)
    list(APPEND AVM_CORE_SOURCES hypervisor/hypervisor_hvf.mm)
    set_source_files_properties(hypervisor/hypervisor_hvf.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

add_library(avm_core STATIC ${AVM_CORE_SOURCES})

target_include_directories(avm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# macOS frameworks
if(AVM_PLATFORM_MACOS)
    target_link_libraries(avm_core PUBLIC
        "-framework Hypervisor"
        "-framework Foundation"
    )
endif()

# Windows WHPX
if(AVM_PLATFORM_WINDOWS AND AVM_ENABLE_WHPX)
    target_compile_definitions(avm_core PUBLIC AVM_ENABLE_WHPX=1)
    target_link_libraries(avm_core PUBLIC WinHvPlatform)
endif()

# Linux KVM
if(AVM_PLATFORM_LINUX AND AVM_ENABLE_KVM)
    target_compile_definitions(avm_core PUBLIC AVM_ENABLE_KVM=1)
endif()
