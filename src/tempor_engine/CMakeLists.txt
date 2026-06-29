

add_library(tempor_class STATIC tempor.cpp api.cpp global_api_symbols.cpp)
target_link_libraries(
    tempor_class PRIVATE logger window_manager hardware_layer_interface output_sink
    plugin_loader scene_graph common file_registry asset_store settings threading
)
target_include_directories(tempor_class PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


