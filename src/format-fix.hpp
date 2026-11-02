#ifndef FORMAT_HPP_
#define FORMAT_HPP_

#if defined(__has_include)
    #if __has_include(<format>)
        #include <format>
    #endif
#endif

namespace format {
    #if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
        using std::format;
    #else
        #include <fmt/format.h>
        using fmt::format;
    #endif
}

#endif  // FORMAT_HPP_
