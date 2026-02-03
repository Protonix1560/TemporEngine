

add_library(scene_manager STATIC scene_manager.cpp)
target_link_libraries(scene_manager PRIVATE logger common)
target_include_directories(scene_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


