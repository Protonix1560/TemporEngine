

add_library(gui STATIC gui_processor.cpp)

target_link_libraries(gui PRIVATE logger irenderer common)

target_include_directories(gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
