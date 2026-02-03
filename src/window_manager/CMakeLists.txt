
find_package(SDL2 REQUIRED)

add_library(window_manager STATIC window_manager.cpp)
target_link_libraries(window_manager PRIVATE SDL2::SDL2 common render_common_types logger)
target_link_libraries(window_manager INTERFACE render_common_types)
target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(window_manager PROPERTIES POSITION_INDEPENDENT_CODE ON)
