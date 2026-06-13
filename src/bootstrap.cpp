

// everything low-level, that is a part of bootstrap sequence or is a generally useful helper is in snake_case or ALL_CAPS
// everything else is in camelCase, PascalCase or ALL_CAPS


#include "core.hpp"

#if !defined(LINUX)
    #error "Unsupported OS"
#endif

#include "tempor.hpp"

#include "arg_parser.hpp"

#include <cstdio>
#include <csignal>
#include <exception>


namespace {
    TemporEngine* g_engine = nullptr;
}


void sigint_handler(int) noexcept {
    if (g_engine) g_engine->sigint();
}
void sigterm_handler(int) noexcept {
    if (g_engine) g_engine->sigterm();
}


int main(int argc, char* argv[]) {

    size_t verbose_level = 0;
    std::string_view config_path;

    try {
        std::ios::sync_with_stdio(false);
        std::signal(SIGINT, sigint_handler);
        std::signal(SIGTERM, sigterm_handler);

        arg_parser parser{};

        auto root_h = parser.define_flag('h', {}, 0, nullptr, "Shows help message. -h: simplified, -hh: advanced");
        auto root_help = parser.define_flag(0, "help", 0, nullptr, "Shows advanced help message");
        auto root_v = parser.define_flag('v', {}, 0, nullptr, "Sets runtime log verbosity. -v: 3, -vv: 4, -vvv: 5");
        auto root_verbose = parser.define_flag(0, "verbose", ARGP_FLAG_HAS_VALUE_FLAG_BIT, nullptr, "Sets runtime log verbosity [0-5]. Overrides -v");
        auto root_config = parser.define_flag('c', "config", ARGP_FLAG_HAS_VALUE_FLAG_BIT, nullptr, "Sets path to config file");

        arg_parser_err argp_err = parser.parse(argc, argv);
        if (argp_err != ARGP_SUCCESS) {
            std::printf("Failed to parse arguments [%d]\n", argp_err);
            return 1;
        }

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
            if (verbose_level > 5) {
                std::fprintf(stderr, "Verbose value %zu is not in [0, 5]\n", verbose_level);
                return 1;
            }
        }

        if (root_config.present()) {
            config_path = root_config.value<std::string_view>(root_config.count() - 1);
        }

    } catch (const std::exception& e) {
        std::printf("\033[91mFailed to parse arguments: %s\n", e.what());
    } catch (...) {
        std::printf("\033[91mFailed to parse arguments\n");
    }

    std::fflush(stdout);

    try {
        TemporEngine engine(verbose_level, std::string(config_path));

        int exit_code;

        exit_code = engine.init();
        if (exit_code != 0) return exit_code;

        exit_code = engine.run();
        if (exit_code != 0) return exit_code;

        engine.shutdown();
    } catch (std::exception& e) {
        std::printf("\033[91mLeaked exception: %s\n", e.what());
        return 1;
    } catch (...) {
        std::printf("\033[91mLeaked exception\n");
        return 1;
    }
    return 0;
}
