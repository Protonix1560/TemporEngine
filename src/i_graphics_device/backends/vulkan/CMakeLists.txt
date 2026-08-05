
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

add_custom_target(vulkan_shaders ALL DEPENDS ${SPV_OUTPUTS})

# layer
add_library(
    vulkan_backend SHARED
    lifecycle.cpp buffer.cpp utility.cpp window_context.cpp asset.cpp allocator.cpp capability.cpp
)
add_dependencies(vulkan_backend vulkan_shaders)
target_link_libraries(
    vulkan_backend PRIVATE
    Vulkan::Vulkan logger windowing scheduler scene_graph common file_registry settings
)
target_include_directories(vulkan_backend PRIVATE ${I_GRAPHICS_DEVICE_SOURCE_DIR})

# linking to i_graphics_device
target_link_libraries(i_graphics_device INTERFACE vulkan_backend)

# installment
# shaders
install(FILES ${SPV_OUTPUTS} DESTINATION shaders/vulkan)
# library
install(TARGETS vulkan_backend LIBRARY DESTINATION lib)

