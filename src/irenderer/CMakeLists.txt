


add_library(render_common_types INTERFACE)
target_include_directories(render_common_types INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(irenderer INTERFACE)
target_include_directories(irenderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(irenderer INTERFACE render_common_types)
file(GLOB RENDERER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/renderers/*")
foreach(DIR ${RENDERER_DIRS})
    if(IS_DIRECTORY ${DIR})
        add_subdirectory(${DIR})
    endif()
endforeach()

