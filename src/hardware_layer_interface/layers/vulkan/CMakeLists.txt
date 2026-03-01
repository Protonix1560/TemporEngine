
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

# renderer
add_library(
    hardware_layer_vulkan SHARED
    hardware_layer.cpp
    swapchain.cpp
)
add_dependencies(hardware_layer_vulkan vulkan_shaders)
target_link_libraries(
    hardware_layer_vulkan PRIVATE
    Vulkan::Vulkan hardware_layer_interface logger window_manager
    common hardware_common_structs resource_registry
)
target_include_directories(hardware_layer_vulkan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# linking to irenderer
target_link_libraries(hardware_layer_interface INTERFACE hardware_layer_vulkan)

# installment
# shaders
install(FILES ${SPV_OUTPUTS} DESTINATION shaders/vulkan)
# library
install(TARGETS hardware_layer_vulkan LIBRARY DESTINATION lib)

