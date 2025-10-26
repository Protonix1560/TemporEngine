
cmake_minimum_required(VERSION 3.25)
project(tempor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TEMPOR_VULKAN_BACKEND "Enable vulkan backend support" ON)
option(TEMPOR_SANITIZER "Enable sanitizer" ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_backends)

add_executable(
    tempor
    src/main.cpp
    src/tempor_engine/tempor.cpp
)

add_subdirectory(src/utils)
target_link_libraries(tempor PRIVATE window_manager)
target_link_libraries(tempor PRIVATE gui_processor)

# including vulkan gpu backend if needed
if (TEMPOR_VULKAN_BACKEND)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_backends/vulkan_backend)
    target_link_libraries(tempor PRIVATE vulkan_backend)
endif()

# including sanitizer if needed
if (TEMPOR_SANITIZER)
    add_link_options(-fsanitize=address)
    add_compile_options(-fsanitize=address)
endif()

add_compile_options(-O0)

target_include_directories(tempor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/tempor_engine)  # for main.cpp to see tempor.hpp


# fastgltf
# include(FetchContent)
# FetchContent_Declare(
#   fastgltf
#   GIT_REPOSITORY https://github.com/spnda/fastgltf.git
#   GIT_TAG        main
# )
# FetchContent_MakeAvailable(fastgltf)
# target_link_libraries(tempor PRIVATE fastgltf::fastgltf)
