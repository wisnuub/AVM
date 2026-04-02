set(AVM_CORE_SOURCES
    vm_manager.cpp
    config.cpp
    android_version.cpp
    image_manager.cpp
    hypervisor/hypervisor_detect.cpp
)

add_library(avm_core STATIC ${AVM_CORE_SOURCES})

target_include_directories(avm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# macOS: link Hypervisor.framework
if(APPLE)
    target_link_libraries(avm_core PUBLIC
        "-framework Hypervisor"
    )
    # Apple Silicon — resolve sysctlbyname for CPU count query
    target_link_libraries(avm_core PUBLIC "-framework CoreFoundation")
endif()

# Windows: Winsock for QMP TCP
if(WIN32)
    target_link_libraries(avm_core PUBLIC ws2_32)
endif()
