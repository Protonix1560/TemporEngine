

find_package(Vulkan REQUIRED)


# compiling shaders

file(
    GLOB SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
)

foreach(SHADER ${SHADERS})
    get_filename_component(NAME ${SHADER} NAME)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/vulkan/${NAME}.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders/vulkan
        COMMAND glslc ${SHADER} -o ${CMAKE_BINARY_DIR}/shaders/vulkan/${NAME}.spv
        DEPENDS ${SHADER}
    )
    list(APPEND SPV_OUTPUTS ${CMAKE_BINARY_DIR}/shaders/vulkan/${NAME}.spv)
endforeach()

add_custom_target(vulkan_shaders DEPENDS ${SPV_OUTPUTS})


# renderer

add_library(
    renderer_vulkan STATIC
    renderer_vulkan.cpp
    swapchain.cpp
)

add_dependencies(renderer_vulkan vulkan_shaders)

target_link_libraries(
    renderer_vulkan
    PRIVATE Vulkan::Vulkan renderer utils window_manager
)

target_include_directories(renderer_vulkan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

