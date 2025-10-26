

find_package(Vulkan REQUIRED)

add_library(
    vulkan_backend STATIC
    vulkan_backend.cpp
    swapchain.cpp
)

target_link_libraries(vulkan_backend
    PRIVATE Vulkan::Vulkan
    PRIVATE window_manager
)

target_include_directories(vulkan_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

