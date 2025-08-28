
cmake_minimum_required(VERSION 3.25)
project(tempor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TEMPOR_VULKAN_BACKEND "Enable vulkan backend support" ON)
option(TEMPOR_SANITIZER "Enable sanitizer" ON)

find_package(SDL2 REQUIRED)

add_executable(
    tempor
    src/main.cpp
    src/tempor.cpp
    src/window_manager.cpp
)

# tmp macros
add_compile_definitions(LSAN_BUIT_IN_OPTIONS TEMPOR_VK_DEBUG_UTILS_MESSENGER)

# link vulkan backend
if (TEMPOR_VULKAN_BACKEND)
    add_subdirectory(src/vulkan)
    target_link_libraries(tempor PRIVATE vulkan_backend)
endif()

# add sanitizer
if (TEMPOR_SANITIZER)
    add_link_options(-fsanitize=address)
    add_compile_options(-fsanitize=address)
endif()

add_compile_options(-O0)

target_link_libraries(tempor PRIVATE SDL2::SDL2)


# fastgltf
# include(FetchContent)
# FetchContent_Declare(
#   fastgltf
#   GIT_REPOSITORY https://github.com/spnda/fastgltf.git
#   GIT_TAG        main
# )
# FetchContent_MakeAvailable(fastgltf)
# target_link_libraries(tempor PRIVATE fastgltf::fastgltf)
