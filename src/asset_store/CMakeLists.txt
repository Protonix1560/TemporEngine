

add_library(asset_store STATIC asset_store.cpp)

target_link_libraries(asset_store PRIVATE common logger file_registry fastgltf::fastgltf i_graphics_device)
target_include_directories(asset_store PUBLIC ${CMAKE_SOURCE_DIR}/3rd_party/fastgltf/include/)

target_include_directories(asset_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

