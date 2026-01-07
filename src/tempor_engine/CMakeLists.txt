

add_library(tempor_api STATIC tempor_api.cpp)
target_link_libraries(tempor_api PRIVATE utils scene_manager common window_manager data_bridge irenderer)
target_include_directories(tempor_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


add_library(tempor_class STATIC tempor.cpp)
target_link_libraries(tempor_class PRIVATE utils window_manager gui irenderer plugin_launcher scene_manager data_bridge common)
target_include_directories(tempor_class PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


