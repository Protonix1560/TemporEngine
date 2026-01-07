
cmake_minimum_required(VERSION 3.25)
project(tempor_test_plugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WANT_DEBUG_ASAN "Enable ASan" OFF)
option(WANT_DEBUG_LSAN "Enable LSan" OFF)
option(WANT_DEBUG_TSAN "Enable TSan" OFF)
option(WANT_DEBUG_MSAN "Enable MSan" OFF)
option(WANT_DEBUG_LSAN "Enable UBSan" OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR "CMAKE_INSTALL_PREFIX is not set")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# main dynamic library
add_library(
    test_plugin SHARED
    plugin.cpp
)

target_include_directories(test_plugin PRIVATE ../../include)

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
install(TARGETS test_plugin LIBRARY DESTINATION plugins/test)

