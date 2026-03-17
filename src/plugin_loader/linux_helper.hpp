
#ifndef PLUGIN_LOADER_LINUX_HELPER_HPP_
#define PLUGIN_LOADER_LINUX_HELPER_HPP_

#include "core.hpp"

#include <filesystem>
#include <dlfcn.h>
#include <cstdint>
#include <type_traits>
#include <cassert>



struct Lib {

    public:

        enum OpenMode : uint32_t {
            OPEN_MODE_LAZY = 0x1
        };

        expected<void, const char*> open(std::filesystem::path path, uint32_t mode = 0) {
            assert(mpFile == nullptr);
            int m = 0;
            if (mode & OPEN_MODE_LAZY) m |= RTLD_LAZY;
            else m |= RTLD_NOW;
            m |= RTLD_LOCAL;
            mpFile = dlopen(path.c_str(), m);
            if (!mpFile) {
                return unexpected(dlerror());
            }
            dlerror();
            return expected_void();
        }

        template <typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
        expected<T, const char*> sym(std::string_view sym) {
            assert(mpFile != nullptr);
            void* p = dlsym(mpFile, sym.data());
            if (!p) {
                return unexpected(dlerror());
            }
            dlerror();
            return reinterpret_cast<T>(p);
        }

        void close() noexcept {
            if (mpFile) {
                dlclose(mpFile);
                mpFile = nullptr;
            }
        }

    private:
        void* mpFile = nullptr;

};


#endif  // PLUGIN_LOADER_LINUX_HELPER_HPP_
