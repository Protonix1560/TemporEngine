
find_package(SDL3 REQUIRED)

add_library(windowing STATIC windowing.cpp)
target_link_libraries(windowing PRIVATE SDL3::SDL3 common logger scheduler)
target_link_libraries(windowing INTERFACE)
target_include_directories(windowing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(windowing PROPERTIES POSITION_INDEPENDENT_CODE ON)
