

add_library(data_bridge STATIC data_bridge.cpp)
target_link_libraries(data_bridge PRIVATE logger common)

target_include_directories(data_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

