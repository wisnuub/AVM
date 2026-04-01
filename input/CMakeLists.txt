add_library(avm_input STATIC
    input_bridge_adb.cpp
    keymapper/keymapper.cpp
)

target_include_directories(avm_input PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
