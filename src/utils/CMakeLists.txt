

# logger
add_library(logger STATIC logger/logger.cpp)
target_link_libraries(logger PUBLIC common)
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logger)
set_target_properties(logger PROPERTIES POSITION_INDEPENDENT_CODE ON)

# vfs
add_library(vfs STATIC vfs/vfs.cpp)
target_link_libraries(vfs PUBLIC logger common)
target_include_directories(vfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vfs)
set_target_properties(vfs PROPERTIES POSITION_INDEPENDENT_CODE ON)

# utils
add_library(utils INTERFACE)
target_link_libraries(utils INTERFACE vfs logger)