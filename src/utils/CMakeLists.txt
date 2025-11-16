

# logger
add_library(logger INTERFACE)
target_include_directories(logger INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/logger)

# io
add_library(io STATIC io/io.cpp)
target_link_libraries(io PUBLIC logger)
target_include_directories(io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/io)

add_library(utils INTERFACE)
target_link_libraries(utils INTERFACE io logger)