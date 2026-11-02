cmake_minimum_required(VERSION 3.21)
project(tempor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
    message(FATAL_ERROR "CMAKE_INSTALL_PREFIX is not set")
endif()

# common things
add_library(common INTERFACE)
target_include_directories(
    common INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rd_party
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/lib
)

function(add_common_option_int option desc default match)
    set(${option} ${default} CACHE STRING "${desc}. Must match \"${match}\"")
    if(NOT "${${option}}" MATCHES "${match}")
        message(FATAL_ERROR "${option} must match \"${match}\"")
    endif()
    target_compile_definitions(common INTERFACE ${option}=${${option}})
endfunction()

function(add_common_option_string option desc default match)
    set(${option} ${default} CACHE STRING "${desc}. Must match \"${match}\"")
    if(NOT "${${option}}" MATCHES "${match}")
        message(FATAL_ERROR "${option} must match \"${match}\"")
    endif()
    target_compile_definitions(common INTERFACE "${option}=\"${${option}}\"")
endfunction()

function(add_common_option_bool option desc default)
    option(${option} ${desc} ${default})
    if(${${option}})
        target_compile_definitions(common INTERFACE ${option})
    endif()
endfunction()

option(TEMPOR_DEBUG_WANT_ASAN "Enable ASan" OFF)
option(TEMPOR_DEBUG_WANT_LSAN "Enable LSan" OFF)
option(TEMPOR_DEBUG_WANT_TSAN "Enable TSan" OFF)
option(TEMPOR_DEBUG_WANT_MSAN "Enable MSan" OFF)
option(TEMPOR_DEBUG_WANT_UBSAN "Enable UBSan" OFF)
add_common_option_int(TEMPOR_FIO_RW_CHUNK_SIZE "Set chunk size in bytes in file_read_all function" 16777215 "^[0-9]+$")

target_compile_options(
    common INTERFACE
    -g -O0 -fno-rtti -DNDEBUG -fno-omit-frame-pointer
    -ffunction-sections -fdata-sections
)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    string(TIMESTAMP BUILD_DATETIME "%Y-%m-%dT%H:%M:%SZ" UTC)
else()
    set(BUILD_DATETIME "2000-12-20T20:20:20Z")
endif()
set(BUILD_VERSION "v0.0.0")
target_compile_definitions(
    common INTERFACE
    BUILD_DATETIME="${BUILD_DATETIME}" BUILD_VERSION="${BUILD_VERSION}"
    SOURCE_ROOT_DIR="${CMAKE_SOURCE_DIR}"
    _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE
)

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

add_sanitizer(address "GNU|Clang" TEMPOR_DEBUG_WANT_ASAN)
add_sanitizer(thread "GNU|Clang" TEMPOR_DEBUG_WANT_TSAN)
add_sanitizer(memory "Clang" TEMPOR_DEBUG_WANT_MSAN -fno-omit-frame-pointer)
add_sanitizer(undefined "GNU|Clang" TEMPOR_DEBUG_WANT_UBSAN -fno-sanitize=vptr)
add_sanitizer(leak "GNU|Clang" TEMPOR_DEBUG_WANT_LSAN -g)

# adding tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# main executable
add_executable(tempor_executable src/bootstrap.cpp)
target_link_libraries(tempor_executable PRIVATE common)
set_target_properties(tempor_executable PROPERTIES
    INSTALL_RPATH "$ORIGIN/lib"
    OUTPUT_NAME tempor
)

# checking if system is posix
include(CheckCSourceCompiles)
check_c_source_compiles("
#include <unistd.h>
int main() { return 0; }
" HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    add_compile_definitions(HAVE_UNISTD_H)
endif()

# adding dependencies
set(FASTGLTF_ENABLE_INSTALL OFF)
add_subdirectory(3rd_party/fastgltf)
target_include_directories(common INTERFACE ${CMAKE_SOURCE_DIR}/3rd_party/fastgltf/include/)
set_target_properties(fastgltf PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(3rd_party/mio)
target_include_directories(common INTERFACE ${CMAKE_SOURCE_DIR}/3rd_party/mio/include/)

# adding services
add_subdirectory(src/i_graphics_device)
add_subdirectory(src/logger)
add_subdirectory(src/filesystem)
add_subdirectory(src/plugin_loader)
add_subdirectory(src/windowing)
add_subdirectory(src/scene_graph)
add_subdirectory(src/asset_store)
add_subdirectory(src/settings)
add_subdirectory(src/scheduler)
add_subdirectory(src/tempor_engine)

target_link_libraries(tempor_executable PRIVATE tempor_engine)

# install
install(TARGETS tempor_executable RUNTIME DESTINATION .)

# test template
install(
    DIRECTORY dist_template/ DESTINATION .
    FILES_MATCHING PATTERN "*.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
install(
    DIRECTORY dist_template/ DESTINATION .
    PATTERN "*.sh" EXCLUDE
)

