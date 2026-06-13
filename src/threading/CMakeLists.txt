
add_library(threading STATIC threading.cpp)
target_link_libraries(threading PRIVATE common logger settings)
target_include_directories(threading PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
