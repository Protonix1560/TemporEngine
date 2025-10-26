
find_package(SDL2 REQUIRED)

add_library(window_manager STATIC
    window_manager/window_manager.cpp
)
target_link_libraries(window_manager PRIVATE SDL2::SDL2)

add_library(gui_processor STATIC
    gui/tpml_parser.cpp
    gui/gui_processor.cpp
)

target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/window_manager)
target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/io)
target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logger)
target_include_directories(window_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gui)

target_include_directories(gui_processor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/window_manager)
target_include_directories(gui_processor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/io)
target_include_directories(gui_processor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logger)
target_include_directories(gui_processor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gui)
