

# logger
add_library(logger STATIC logger.cpp)
target_link_libraries(logger PUBLIC common)
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(logger PROPERTIES POSITION_INDEPENDENT_CODE ON)
