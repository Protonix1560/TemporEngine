
cmake_minimum_required(VERSION 3.25)
project(tempor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WANT_DEBUG_ASAN "Enable ASan" OFF)
option(WANT_DEBUG_LSAN "Enable LSan" OFF)
option(WANT_DEBUG_TSAN "Enable TSan" OFF)
option(WANT_DEBUG_MSAN "Enable MSan" OFF)
option(WANT_DEBUG_LSAN "Enable UBSan" OFF)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/3rd_party
    ${CMAKE_SOURCE_DIR}/include
)

add_executable(tempor src/main.cpp)
add_compile_options(-O0)

# adding main class
add_subdirectory(src/tempor_engine)
target_link_libraries(tempor PRIVATE tempor_class)

# adding utils
add_subdirectory(src/utils)
target_link_libraries(tempor PRIVATE utils)

# adding plugin laucher
add_subdirectory(src/plugin_launcher)
target_link_libraries(tempor PRIVATE plugin_launcher)

# adding window manager
add_subdirectory(src/window_manager)
target_link_libraries(tempor PRIVATE window_manager)

# adding graphics API
add_subdirectory(src/renderer)
target_link_libraries(tempor PRIVATE renderer)

# adding gui
add_subdirectory(src/gui)
target_link_libraries(tempor PRIVATE gui)

# adding sanitizers
function(add_sanitizer sanitizer supported_compilers want_flag)
    if(${want_flag})
        if(CMAKE_CXX_COMPILER_ID MATCHES "${supported_compilers}")
            set("${status_flag}" "Enabled" PARENT_SCOPE)
            message(STATUS "Enabling ${sanitizer} sanitizer")

            string(REPLACE ";" " " additional_flags "${ARGN}")

            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=${sanitizer} ${additional_flags}" PARENT_SCOPE)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=${sanitizer} ${additional_flags}" PARENT_SCOPE)

            set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=${sanitizer}" PARENT_SCOPE)
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=${sanitizer}" PARENT_SCOPE)

        else()
            set("${status_flag}" "Wanted but disabled due to unsupported compiler" PARENT_SCOPE)
            message(STATUS "${sanitizer} sanitizer wanted but compiler not supported")
        endif()
    else()
        set("${status_flag}" "Disabled" PARENT_SCOPE)
    endif()
endfunction()

add_sanitizer(address "GNU|Clang" WANT_DEBUG_ASAN)
add_sanitizer(thread "GNU|Clang" WANT_DEBUG_TSAN)
add_sanitizer(memory "Clang" WANT_DEBUG_MSAN -fno-omit-frame-pointer)
add_sanitizer(undefined "GNU|Clang" WANT_DEBUG_UBSAN -fno-sanitize=vptr)
add_sanitizer(leak "GNU|Clang" WANT_DEBUG_LSAN -g)

# adding test plugin
add_subdirectory(test_plugin)


