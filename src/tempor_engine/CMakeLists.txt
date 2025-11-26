
add_library(tempor_class STATIC tempor.cpp tempor_api.cpp)

target_link_libraries(tempor_class PRIVATE utils window_manager gui renderer plugin_launcher scene_manager)

target_include_directories(tempor_class PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


