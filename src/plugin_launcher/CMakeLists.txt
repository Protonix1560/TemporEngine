

add_library(
    plugin_launcher STATIC
    plugin_launcher.cpp
)

target_link_libraries(plugin_launcher PRIVATE data_bridge logger common)

target_include_directories(plugin_launcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


