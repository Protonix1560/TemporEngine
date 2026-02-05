

add_library(tempor_api STATIC tempor_api.cpp)
target_link_libraries(
    tempor_api PRIVATE logger scene_manager common window_manager
    data_bridge hardware_layer_interface resource_registry asset_store
)
target_include_directories(tempor_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


add_library(tempor_class STATIC tempor.cpp)
target_link_libraries(
    tempor_class PRIVATE logger window_manager gui hardware_layer_interface
    plugin_launcher scene_manager data_bridge common resource_registry asset_store
    hardware_memory_optimizator
)
target_include_directories(tempor_class PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


