add_library(avm_overlay STATIC
    overlay.cpp
)

target_include_directories(avm_overlay PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Dear ImGui (CDN / submodule — add as submodule later)
# add_subdirectory(vendor/imgui)
# target_link_libraries(avm_overlay PRIVATE imgui)
