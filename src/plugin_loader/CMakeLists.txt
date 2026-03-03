

add_library(plugin_wrapper STATIC plugin_wrapper.cpp)
target_link_libraries(plugin_wrapper PRIVATE common)
target_include_directories(plugin_wrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


add_library(plugin_loader STATIC plugin_loader.cpp)
target_link_libraries(plugin_loader PRIVATE logger common plugin_wrapper)
target_include_directories(plugin_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


