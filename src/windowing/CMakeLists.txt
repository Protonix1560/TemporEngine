
find_package(SDL3 REQUIRED)

add_library(windowing STATIC windowing.cpp)
target_link_libraries(windowing PRIVATE SDL3::SDL3 common logger scheduler)
target_include_directories(windowing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(windowing PRIVATE ${CMAKE_SOURCE_DIR}/src/i_graphics_device/)
set_target_properties(windowing PROPERTIES POSITION_INDEPENDENT_CODE ON)
