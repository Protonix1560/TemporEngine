
add_library(filesystem STATIC filesystem.cpp)
target_include_directories(filesystem PUBLIC ${CMAKE_SOURCE_DIR}/3rd_party/ELFIO/)
target_link_libraries(filesystem PRIVATE common logger)
set_target_properties(filesystem PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
