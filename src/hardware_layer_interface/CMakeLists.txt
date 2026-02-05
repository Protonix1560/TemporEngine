


add_library(hardware_common_structs INTERFACE)
target_include_directories(hardware_common_structs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(hardware_layer_interface INTERFACE)
target_include_directories(hardware_layer_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hardware_layer_interface INTERFACE hardware_common_structs)
file(GLOB RENDERER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/layers/*")
foreach(DIR ${RENDERER_DIRS})
    if(IS_DIRECTORY ${DIR})
        add_subdirectory(${DIR})
    endif()
endforeach()

