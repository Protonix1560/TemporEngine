
add_library(output_sink STATIC output_sink.cpp)
target_link_libraries(output_sink PRIVATE common settings file_registry logger)
target_include_directories(output_sink PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
