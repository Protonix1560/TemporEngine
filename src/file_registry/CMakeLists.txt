
add_library(file_registry STATIC file_registry.cpp)
target_include_directories(file_registry PUBLIC ${CMAKE_SOURCE_DIR}/3rd_party/ELFIO/)
target_link_libraries(file_registry PRIVATE common logger)
set_target_properties(file_registry PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(file_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
