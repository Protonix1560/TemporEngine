

option(WANT_VULKAN "Enable Vulkan graphics API support" ON)

add_library(renderer INTERFACE)

target_include_directories(renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# adding Vulkan graphics API support
if (WANT_VULKAN)
    add_subdirectory(renderer_vulkan)
    target_link_libraries(renderer INTERFACE renderer_vulkan)
endif()

