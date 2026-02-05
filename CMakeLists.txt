
cmake_minimum_required(VERSION 3.25)
project(tempor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WANT_DEBUG_ASAN "Enable ASan" OFF)
option(WANT_DEBUG_LSAN "Enable LSan" OFF)
option(WANT_DEBUG_TSAN "Enable TSan" OFF)
option(WANT_DEBUG_MSAN "Enable MSan" OFF)
option(WANT_DEBUG_LSAN "Enable UBSan" OFF)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
    message(FATAL_ERROR "CMAKE_INSTALL_PREFIX is not set")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# common things
add_library(common INTERFACE)
target_include_directories(
    common INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rd_party
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(common INTERFACE -O0)
string(TIMESTAMP BUILD_DATETIME "%Y-%m-%dT%H:%M:%SZ" UTC)
target_compile_definitions(common INTERFACE BUILD_DATETIME="${BUILD_DATETIME}")

# main executable
add_executable(tempor src/main.cpp)
target_link_libraries(tempor PRIVATE common)
set_target_properties(tempor PROPERTIES
    INSTALL_RPATH "$ORIGIN/lib"
)

# adding services
add_subdirectory(src/hardware_layer_interface)
add_subdirectory(src/hardware_memory_optimizator)
add_subdirectory(src/tempor_engine)
add_subdirectory(src/logger)
add_subdirectory(src/data_bridge)
add_subdirectory(src/resource_registry)
add_subdirectory(src/plugin_launcher)
add_subdirectory(src/window_manager)
add_subdirectory(src/gui)
add_subdirectory(src/scene_manager)
add_subdirectory(src/asset_store)

target_link_libraries(
    tempor PRIVATE
    tempor_class tempor_api logger plugin_launcher window_manager scene_manager
    hardware_layer_interface gui data_bridge resource_registry asset_store hardware_memory_optimizator
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

add_sanitizer(address "GNU|Clang" WANT_DEBUG_ASAN)
add_sanitizer(thread "GNU|Clang" WANT_DEBUG_TSAN)
add_sanitizer(memory "Clang" WANT_DEBUG_MSAN -fno-omit-frame-pointer)
add_sanitizer(undefined "GNU|Clang" WANT_DEBUG_UBSAN -fno-sanitize=vptr)
add_sanitizer(leak "GNU|Clang" WANT_DEBUG_LSAN -g)

# installment
install(TARGETS tempor RUNTIME DESTINATION .)

# test template
install(
    DIRECTORY test_template/ DESTINATION .
    FILES_MATCHING PATTERN "*.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
install(
    DIRECTORY test_template/ DESTINATION .
    PATTERN "*.sh" EXCLUDE
)

