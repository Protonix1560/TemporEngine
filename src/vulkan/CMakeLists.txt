

find_package(Vulkan REQUIRED)

add_library(
    vulkan_backend STATIC
    backend.cpp
    swapchain.cpp
)

target_link_libraries(vulkan_backend PRIVATE Vulkan::Vulkan)

