

add_library(tempor_engine STATIC tempor.cpp api.cpp global_api_symbols.cpp)
target_link_libraries(
    tempor_engine PUBLIC logger window_manager hardware_layer_interface output_sink
    plugin_loader scene_graph common file_registry asset_store settings scheduler
)
target_include_directories(tempor_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


