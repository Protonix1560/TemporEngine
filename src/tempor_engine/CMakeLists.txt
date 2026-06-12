

add_library(tempor_class STATIC tempor.cpp)
target_link_libraries(
    tempor_class PRIVATE logger window_manager hardware_layer_interface
    plugin_loader scene_graph common resource_registry asset_store settings threading
)
target_include_directories(tempor_class PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


