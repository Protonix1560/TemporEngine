
find_package(SDL2 REQUIRED)

add_library(window_manager STATIC window_manager.cpp)

target_link_libraries(window_manager PRIVATE SDL2::SDL2)

target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

