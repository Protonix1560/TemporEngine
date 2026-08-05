
add_library(i_graphics_device INTERFACE)
target_include_directories(i_graphics_device INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(I_GRAPHICS_DEVICE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB RENDERER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/backends/*")
foreach(DIR ${RENDERER_DIRS})
    if(IS_DIRECTORY ${DIR})
        add_subdirectory(${DIR})
    endif()
endforeach()

