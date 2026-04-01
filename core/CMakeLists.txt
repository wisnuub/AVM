add_library(avm_core STATIC
    vm_manager.cpp
    hypervisor/hypervisor_detect.cpp
    hypervisor/kvm_backend.cpp
    hypervisor/whpx_backend.cpp
)

target_include_directories(avm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(AVM_PLATFORM_LINUX AND AVM_ENABLE_KVM)
    target_compile_definitions(avm_core PRIVATE AVM_KVM_ENABLED)
endif()

if(AVM_PLATFORM_WINDOWS AND AVM_ENABLE_WHPX)
    target_compile_definitions(avm_core PRIVATE AVM_WHPX_ENABLED)
endif()
