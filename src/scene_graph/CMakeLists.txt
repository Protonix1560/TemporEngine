

find_library(XXHASH_LIB NAMES xxhash)

add_library(scene_graph STATIC scene_graph.cpp)
target_link_libraries(scene_graph PRIVATE logger common xxhash)
target_include_directories(scene_graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


