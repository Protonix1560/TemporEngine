

#if !defined(__linux__)
    #error "Unsupported OS type"
#endif


// everything in snake_case is considered to be low-level
// everything in camelCase or PascalCase is considered to be high-level


#include "core.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "tempor.hpp"
#include "arg_parser.hpp"

#include <cstdio>
#include <type_traits>
#include <csignal>



namespace {
    TemporEngine* g_engine = nullptr;

    namespace api {


        template <typename F, typename E = void, typename... Args>
        class std_handler {
            public:
                std_handler(F&& f, Args&&... args) {
                    static_assert(dependent_false_v<F>, "Unsupported type");
                }
        };

        // for TprResult
        template <typename F, typename... Args>
        class std_handler<F, std::enable_if_t<std::is_same_v<TprResult, std::invoke_result_t<F, Args...>>>, Args...> {
            private:
                TprResult ret;
            public:
                std_handler(F&& f, Args&&... args) noexcept {
                    try {
                        assert(g_engine != nullptr);
                        ret = std::forward<F>(f)(std::forward<Args>(args)...);

                    } catch (const std::bad_alloc& e) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Bad alloc: "<< e.what() << "\n";
                        ret = TPR_BAD_ALLOC;

                    } catch (const std::exception& e) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Exception: " << e.what() << "\n";
                        ret = TPR_UNKNOWN_ERROR;

                    } catch (...) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                        ret = TPR_UNKNOWN_ERROR;
                    }
                }
                TprResult operator()() { return ret; }
        };

        // for void
        template <typename F, typename... Args>
        class std_handler<F, std::enable_if_t<std::is_same_v<void, std::invoke_result_t<F, Args...>>>, Args...> {
            public:
                std_handler(F&& f, Args&&... args) noexcept {
                    try {
                        assert(g_engine != nullptr);
                        std::forward<F>(f)(std::forward<Args>(args)...);

                    } catch (const std::bad_alloc& e) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Bad alloc: "<< e.what() << "\n";

                    } catch (const std::exception& e) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Exception: " << e.what() << "\n";

                    } catch (...) {
                        g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                    }
                }
                void operator()() {}
        };


        namespace log {
            void log(TprLogLevel logLevel, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().log(logLevel) << message;
            })(); }

            void error(const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().error(TPR_LOG_STYLE_ERROR1) << message;
            })(); }

            void warn(const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().warn(TPR_LOG_STYLE_WARN1) << message;
            })(); }

            void info(const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().info() << message;
            })(); }

            void debug(const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().debug() << message;
            })(); }

            void trace(const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().trace() << message;
            })(); }

            void logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().log(logLevel, logStyle) << message;
            })(); }

            void errorStyled(TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().error(logStyle) << message;
            })(); }

            void warnStyled(TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().warn(logStyle) << message;
            })(); }

            void infoStyled(TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().info(logStyle) << message;
            })(); }

            void debugStyled(TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().debug(logStyle) << message;
            })(); }

            void traceStyled(TprLogStyle logStyle, const char* message) noexcept { return std_handler([=] {
                g_engine->getLogger().trace(logStyle) << message;
            })(); }
        }

        namespace vfs {
            TprResult openPathResource(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.openResource(std::filesystem::path(path), flags, alignment);
                if (!exp.has_value()) return exp.error();
                *pResource = exp.value();
                return TPR_SUCCESS;
            })(); }

            TprResult openReferenceResource(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.openResource(reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags);
                if (!exp.has_value()) return exp.error();
                *pResource = exp.value();
                return TPR_SUCCESS;
            })(); }

            TprResult openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.openResource(size, flags, alignment);
                if (!exp.has_value()) return exp.error();
                *pResource = exp.value();
                return TPR_SUCCESS;
            })(); }

            TprResult openCapabilityResource(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.openResource(protectResource, flags, protectFlags);
                if (!exp.has_value()) return exp.error();
                *pResource = exp.value();
                return TPR_SUCCESS;
            })(); }

            TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                return rreg.resizeResource(resource, newSize);
            })(); }

            TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.sizeofResource(resource);
                if (!exp.has_value()) return exp.error();
                *pSize = exp.value();
                return TPR_SUCCESS;
            })(); }

            TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.getResourceRawDataPointer(resource);
                if (!exp.has_value()) return exp.error();
                *pData = reinterpret_cast<char*>(exp.value());
                return TPR_SUCCESS;
            })(); }

            TprResult getResourceConstPointer(TprResource resource, const char** pData) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                auto exp = rreg.getResourceConstPointer(resource);
                if (!exp.has_value()) return exp.error();
                *pData = reinterpret_cast<const char*>(exp.value());
                return TPR_SUCCESS;
            })(); }

            void closeResource(TprResource resource) noexcept { return std_handler([=]() {
                ResourceRegistry& rreg = g_engine->getResourceRegistry();
                rreg.closeResource(resource);
            })(); }

        }

    }

}



enum bootstrap_err_code : int {
    bootstrap_success = 0,
    bootstrap_engine_lifetime_violation = 1,
    bootstrap_unhandled_exception = 2,
    bootstrap_invalid_argv = 3
};



void sigint_handler(int) noexcept {
    if (g_engine) g_engine->sigint();
}

void sigterm_handler(int) noexcept {
    if (g_engine) g_engine->sigterm();
}



int main(int argc, char* argv[]) {

    size_t verbose_level = 0;

    TprEngineAPI::Log apiLog;
    apiLog.log = api::log::log;
    apiLog.info = api::log::info;
    apiLog.warn = api::log::warn;
    apiLog.error = api::log::error;
    apiLog.debug = api::log::debug;
    apiLog.trace = api::log::trace;
    apiLog.logStyled = api::log::logStyled;
    apiLog.infoStyled = api::log::infoStyled;
    apiLog.warnStyled = api::log::warnStyled;
    apiLog.errorStyled = api::log::errorStyled;
    apiLog.debugStyled = api::log::debugStyled;
    apiLog.traceStyled = api::log::traceStyled;

    TprEngineAPI::VFS apiVFS;
    apiVFS.openPathResource = api::vfs::openPathResource;
    apiVFS.openReferenceResource = api::vfs::openReferenceResource;
    apiVFS.openEmptyResource = api::vfs::openEmptyResource;
    apiVFS.openCapabilityResource = api::vfs::openCapabilityResource;
    apiVFS.resizeResource = api::vfs::resizeResource;
    apiVFS.sizeofResource = api::vfs::sizeofResource;
    apiVFS.getResourceRawDataPointer = api::vfs::getResourceRawDataPointer;
    apiVFS.getResourceConstPointer = api::vfs::getResourceConstPointer;
    apiVFS.closeResource = api::vfs::closeResource;

    TprEngineAPI::Scene apiScene;

    TprEngineAPI::Geo apiGeo;
    
    TprEngineAPI::WM apiWM;

    TprEngineAPI api;
    api.log = &apiLog;
    api.wm = &apiWM;
    api.vfs = &apiVFS;
    api.scene = &apiScene;
    api.geo = &apiGeo;

    try {

        std::ios::sync_with_stdio(false);
        std::signal(SIGINT, sigint_handler);
        std::signal(SIGTERM, sigterm_handler);

        arg_parser parser{};

        auto root_h = parser.define_flag('h', {}, 0, nullptr, "Shows help message. -h: simplified, -hh: advanced");
        auto root_help = parser.define_flag(0, "help", 0, nullptr, "Shows advanced help message");
        auto root_v = parser.define_flag('v', {}, 0, nullptr, "Sets runtime log verbosity. -v: 3, -vv: 4, -vvv: 5");
        auto root_verbose = parser.define_flag(0, "verbose", FLAG_HAS_VALUE_FLAG_BIT, nullptr, "Sets runtime log verbosity [0-5]. Overrides -v");

        parser.parse(argc, argv);

        if (root_help.present()) {
            parser.print_help_advanced("Tempor - a game engine", "tempor");
            return 0;
        }
        if (root_h.count() == 1) {
            parser.print_help("tempor");
            return 0;
        } else if (root_h.count() >= 2) {
            parser.print_help_advanced("Tempor - a game engine", "tempor");
            return 0;
        }

        if (root_v.count() == 1) {
            verbose_level = 3;
        } else if (root_v.count() == 2) {
            verbose_level = 4;
        } else if (root_v.count() >= 3) {
            verbose_level = 5;
        }
        if (root_verbose.present()) {
            verbose_level = root_verbose.value<size_t>(root_verbose.count() - 1);
            if (verbose_level < 0 || verbose_level > 6) {
                std::fprintf(stderr, "--verbose value is not in [0-5]: %ld\n", verbose_level);
                return bootstrap_invalid_argv;
            }
        }

    } catch (const std::exception& e) {
        std::printf("Parsing error: %s\n", e.what());
    }

    /*
            END OF STDIO-ONLY REGION
    */
    std::fflush(stdout);

    alignas(TemporEngine) static std::byte engine_mem[sizeof(TemporEngine)];

    try {
        g_engine = new (engine_mem) TemporEngine(verbose_level, &api);

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to construct engine: %s\n", e.what());
        std::fflush(stderr);
        return bootstrap_engine_lifetime_violation;
    } catch (...) {
        std::fprintf(stderr, "Failed to construct engine\n");
        std::fflush(stderr);
        return bootstrap_engine_lifetime_violation;
    }

    int exit_code = 0;

    try {

        g_engine->init();
        exit_code = g_engine->run();
        g_engine->shutdown();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Unhandled exception detected: %s\n", e.what());
        std::fflush(stderr);
        exit_code = bootstrap_unhandled_exception;
    } catch (...) {
        std::fprintf(stderr, "Unhandled exception detected\n");
        std::fflush(stderr);
        exit_code = bootstrap_unhandled_exception;
    }

    g_engine->~TemporEngine();

    return exit_code;
}
