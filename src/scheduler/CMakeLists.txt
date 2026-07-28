
add_library(scheduler STATIC scheduler.cpp)
target_link_libraries(scheduler PRIVATE common logger settings)
target_include_directories(scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
