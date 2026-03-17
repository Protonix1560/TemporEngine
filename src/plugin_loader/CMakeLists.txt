


add_library(plugin_loader STATIC plugin_loader.cpp plugin.cpp)
target_link_libraries(plugin_loader PRIVATE logger common)

target_include_directories(plugin_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


