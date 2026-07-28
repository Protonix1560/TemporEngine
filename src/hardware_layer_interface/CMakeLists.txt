
add_library(hardware_layer_interface INTERFACE)
target_include_directories(hardware_layer_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(HARDWARE_LAYER_INTERFACE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB RENDERER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/layers/*")
foreach(DIR ${RENDERER_DIRS})
    if(IS_DIRECTORY ${DIR})
        add_subdirectory(${DIR})
    endif()
endforeach()

