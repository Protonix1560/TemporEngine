
add_library(settings STATIC settings.cpp)
target_link_libraries(settings PRIVATE common resource_registry logger simdjson)
target_include_directories(settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(settings PROPERTIES POSITION_INDEPENDENT_CODE ON)
