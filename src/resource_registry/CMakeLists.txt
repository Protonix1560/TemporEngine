

add_library(resource_registry STATIC resource_registry.cpp)
target_include_directories(resource_registry PUBLIC ${CMAKE_SOURCE_DIR}/3rd_party/mio/include/)
target_include_directories(resource_registry PUBLIC ${CMAKE_SOURCE_DIR}/3rd_party/ELFIO/)
target_link_libraries(resource_registry PRIVATE common logger)
set_target_properties(resource_registry PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(resource_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
