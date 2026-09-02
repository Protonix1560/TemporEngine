
# logger
add_library(logger STATIC logger.cpp)
target_link_libraries(logger PRIVATE common)
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(logger PROPERTIES POSITION_INDEPENDENT_CODE ON)

# log sink
add_library(output_sink STATIC output_sink.cpp)
target_link_libraries(output_sink PRIVATE common logger)
target_include_directories(output_sink PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(output_sink PROPERTIES POSITION_INDEPENDENT_CODE ON)
