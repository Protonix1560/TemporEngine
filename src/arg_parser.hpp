

#ifndef ARG_PARSER_HPP_
#define ARG_PARSER_HPP_


#include <cstdint>
#include <string_view>
#include <deque>
#include <cstring>
#include <stdexcept>
#include <charconv>




enum pos_arg_flags : uint32_t {
    POS_ALL_REQUIRED_FLAG_BIT = 0x1,
    POS_AT_LEAST_ONE_FLAG_BIT = 0x2
};

enum flag_flags : uint32_t {
    FLAG_HAS_VALUE_FLAG_BIT = 0x1
};

enum arg_parser_err : int32_t {
    ARG_PARSER_SUCCESS = 0,
    ARG_PARSER_FLAG_NO_VALUE = -1,
    ARG_PARSER_UNKNOWN_FLAG = -2,
    ARG_PARSER_POS_OVERFLOW = -3,
    ARG_PARSER_NO_REQUIRED_POSITIONAL = -4
};

class arg_parser {
    public:
        using flag_type = uint32_t;

    private:

        struct command;

        struct positional {

            positional(std::string_view name, size_t index, size_t max_words, command& comm, flag_type flags, std::string_view help)
                : name(name), index(index), max_words(max_words), comm(comm), flags(flags), help(help) {}

            std::string_view name;
            size_t index;
            size_t max_words;
            command& comm;
            flag_type flags;
            std::string_view help;

            std::deque<std::string_view> storage;
        };

        struct flag {

            flag(char short_name, std::string_view long_name, command& comm, flag_type flags, std::string_view help)
                : short_name(short_name), long_name(long_name), comm(comm), flags(flags), help(help) {}

            char short_name;
            std::string_view long_name;
            command& comm;
            flag_type flags;
            std::string_view help;

            std::deque<std::string_view> storage;
            size_t count = 0;
        };

        struct command {

            command() : parent_comm(nullptr) {};

            command(std::string_view name, command* parent_command, std::string_view help)
                : name(name), parent_comm(parent_command), help(help) {}

            std::string_view name;
            command* parent_comm;
            std::string_view help;

            // deque, so poiters are stable on growth
            std::deque<command> subcommands;
            std::deque<flag> flags;
            std::deque<positional> positionals;
        };

    public:

        arg_parser() = default;

        struct positional_ref {
            public:
                bool present() const { return m_ref.storage.size() > 0; }
                size_t count() const { return m_ref.storage.size(); }
                std::string_view storage(size_t index) const { return m_ref.storage[index]; }

            private:
                positional_ref(positional& ref) : m_ref(ref) {}
                positional& m_ref;
                friend class arg_parser;
        };

        struct flag_ref {
            public:
                bool present() const { return m_ref.count > 0; }
                size_t count() const { return m_ref.count; }
                std::string_view storage(size_t index) const { return m_ref.storage[index]; }

                template <typename T>
                T value(size_t index) const {
                    std::string_view v = storage(index);
                    if constexpr (std::is_same_v<T, std::string_view>) {
                        return v;

                    } else if constexpr (std::is_same_v<bool, T>) {
                        char* lowercase = static_cast<char*>(operator new[](v.size() + 1));
                        for (size_t i = 0; i < v.size(); i++) {
                            lowercase[i] = static_cast<char>(std::tolower(static_cast<int>(v[i])));
                        }
                        lowercase[v.size()] = '\0';
                        bool val;
                        if (
                            std::strcmp(lowercase, "yes") == 0 ||
                            std::strcmp(lowercase, "y") == 0 ||
                            std::strcmp(lowercase, "on") == 0 ||
                            std::strcmp(lowercase, "true") == 0 ||
                            std::strcmp(lowercase, "t") == 0 ||
                            std::strcmp(lowercase, "yeah") == 0 ||
                            std::strcmp(lowercase, "okay") == 0 ||
                            std::strcmp(lowercase, "ok") == 0 ||
                            std::strcmp(lowercase, "good") == 0 ||
                            std::strcmp(lowercase, "always") == 0 ||
                            std::strcmp(lowercase, "1") == 0
                        ) {
                            val = true;
                            goto converted_bool;

                        } else if (
                            std::strcmp(lowercase, "no") == 0 ||
                            std::strcmp(lowercase, "n") == 0 ||
                            std::strcmp(lowercase, "off") == 0 ||
                            std::strcmp(lowercase, "false") == 0 ||
                            std::strcmp(lowercase, "f") == 0 ||
                            std::strcmp(lowercase, "nope") == 0 ||
                            std::strcmp(lowercase, "bad") == 0 ||
                            std::strcmp(lowercase, "never") == 0 ||
                            std::strcmp(lowercase, "0") == 0
                        ) {
                            val = false;
                            goto converted_bool;
                        }
                        // failed
                        operator delete[](lowercase);
                        throw std::runtime_error("arg_parser: invalid boolean");

                        converted_bool:
                        operator delete[](lowercase);
                        return val;

                    } else if constexpr (std::is_integral_v<T>) {
                        T value{};
                        auto [ptr, ec] = std::from_chars(v.begin(), v.end(), value);
                        if (ec == std::errc{}) {
                            if (ptr != v.end()) throw std::runtime_error("arg_parser: invalid integer");
                            return value;
                        } else if (ec == std::errc::result_out_of_range) throw std::runtime_error("arg_parser: out of range");
                        throw std::runtime_error("arg_parser: invalid integer");
                    } else {
                        throw std::runtime_error("arg_parser: unsupported type");
                    }
                    return T{};
                }

            private:
                flag_ref(flag& ref) : m_ref(ref) {}
                flag& m_ref;
                friend class arg_parser;
        };

        struct command_ref {
            private:
                command_ref(command& ref) : m_ref(ref) {}
                command& m_ref;
                friend class arg_parser;
        };

        command_ref define_subcommand(std::string_view name, command_ref* parent_command = nullptr, std::string_view help = {}) {
            command& comm = get_command(parent_command);
            comm.subcommands.emplace_back(name, &comm, help);
            return command_ref(comm.subcommands.back());
        }

        positional_ref define_positional(std::string_view name, flag_type flags = 0, size_t max_word_count = 1, command_ref* command = nullptr, std::string_view help = {}) {
            arg_parser::command& comm = get_command(command);
            comm.positionals.emplace_back(name, comm.positionals.size(), max_word_count, comm, flags, help);
            return positional_ref(comm.positionals.back());
        }

        flag_ref define_flag(char short_name, std::string_view long_name, flag_type flags = 0, command_ref* command = nullptr, std::string_view help = {}) {
            arg_parser::command& comm = get_command(command);
            comm.flags.emplace_back(short_name, long_name, comm, flags, help);
            return flag_ref(comm.flags.back());
        }

        arg_parser_err parse(int argc, char* argv[]) {

            command* curr_comm = &root_command;
            size_t positional_count = 0;
            size_t complex_flag_sym = 1;

            for (int i = 1; i < argc; i++) {

                char* arg = argv[i];
                size_t arg_size = std::strlen(arg);
                if (arg_size >= 3 && arg[0] == '-' && arg[1] == '-') {
                    // long flag
                    const char* equal_pos;  // needed later in case of an error

                    for (auto& flag : curr_comm->flags) {
                        if (!flag.long_name.empty()) {
                            size_t flag_size = flag.long_name.size();
                            if (std::strncmp(arg + 2, flag.long_name.data(), flag_size) == 0) {
                                if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                                    if (arg_size >= 4 + flag_size && arg[flag_size + 2] == '=') {
                                            auto parsed = parse_commas(arg + flag_size + 3);
                                            flag.storage.insert(flag.storage.end(), parsed.begin(), parsed.end());
                                            flag.count += parsed.size();
                                    } else if (arg_size == 2 + flag_size) {
                                        if (i + 1 < argc) {
                                            auto parsed = parse_commas(argv[i + 1]);
                                            flag.storage.insert(flag.storage.end(), parsed.begin(), parsed.end());
                                            flag.count += parsed.size();
                                            i++;
                                        } else {
                                            std::fprintf(stderr, "arg_parser: no value provided for flag --%s\n", flag.long_name.data());
                                            return ARG_PARSER_FLAG_NO_VALUE;
                                        }
                                    } else {
                                        std::fprintf(stderr, "arg_parser: no value provided for flag --%s\n", flag.long_name.data());
                                        return ARG_PARSER_FLAG_NO_VALUE;
                                    }
                                } else {
                                    flag.count++;
                                }
                                goto found_long_flag;
                            }
                        }
                    }
                    equal_pos = std::strchr(arg + 2, '=');
                    if (equal_pos) {
                        std::fprintf(stderr, "arg_parser: unknown flag --%.*s\n", static_cast<int>(equal_pos - arg - 2), arg + 2);
                    } else {
                        std::fprintf(stderr, "arg_parser: unknown flag --%s\n", arg + 2);
                    }
                    return ARG_PARSER_UNKNOWN_FLAG;
                    found_long_flag: ;

                } else if (arg_size >= 2 && arg[0] == '-') {
                    // short flag
                    for (auto& flag : curr_comm->flags) {
                        if (flag.short_name) {
                            if (arg[complex_flag_sym] == flag.short_name) {
                                if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                                    if (arg_size >= 3 + complex_flag_sym && arg[complex_flag_sym + 1] == '=') {
                                            auto parsed = parse_commas(arg + 2 + complex_flag_sym);
                                            flag.storage.insert(flag.storage.end(), parsed.begin(), parsed.end());
                                            flag.count += parsed.size();

                                    } else if (arg_size >= 2 + complex_flag_sym) {
                                            auto parsed = parse_commas(arg + 1 + complex_flag_sym);
                                            flag.storage.insert(flag.storage.end(), parsed.begin(), parsed.end());
                                            flag.count += parsed.size();
                                            
                                    } else if (arg_size == 1 + complex_flag_sym) {
                                        if (i + 1 < argc) {
                                            auto parsed = parse_commas(argv[i + 1]);
                                            flag.storage.insert(flag.storage.end(), parsed.begin(), parsed.end());
                                            flag.count += parsed.size();
                                            i++;
                                        } else {
                                            std::fprintf(stderr, "arg_parser: no value provided for flag -%c\n", flag.short_name);
                                            return ARG_PARSER_FLAG_NO_VALUE;
                                        }
                                    } else {
                                        std::fprintf(stderr, "arg_parser: no value provided for flag -%c\n", flag.short_name);
                                        return ARG_PARSER_FLAG_NO_VALUE;
                                    }
                                    complex_flag_sym = 1;

                                } else {
                                    if (complex_flag_sym + 1 < arg_size) {
                                        // pass this argument another time
                                        i--;
                                        complex_flag_sym++;
                                    } else {
                                        complex_flag_sym = 1;
                                    }
                                    flag.count++;
                                }
                                goto found_short_flag;
                            }
                        }
                    }
                    std::fprintf(stderr, "arg_parser: unknown flag -%c\n", arg[complex_flag_sym]);
                    return ARG_PARSER_UNKNOWN_FLAG;
                    found_short_flag: ;

                } else {
                    for (auto& command : curr_comm->subcommands) {
                        if (command.parent_comm == curr_comm) {
                            if (std::strcmp(arg, command.name.data()) == 0) {
                                // command
                                curr_comm = &command;
                                positional_count = 0;
                                goto found_subcommand;
                            }
                        }
                    }

                    // positional
                    if (positional_count < curr_comm->positionals.size()) {
                        positional& pos = curr_comm->positionals[positional_count];
                        pos.storage.push_back(arg);
                        if (pos.storage.size() == pos.max_words) {
                            positional_count++;
                        }
                    } else {
                        std::fprintf(stderr, "arg_parser: too much positional arguments");
                        return ARG_PARSER_POS_OVERFLOW;
                    }

                    found_subcommand: ;

                }

            }

            while (curr_comm != nullptr) {

                for (const auto& pos : curr_comm->positionals) {
                    if ((pos.flags & POS_AT_LEAST_ONE_FLAG_BIT && pos.storage.size() < 1) || (pos.flags & POS_ALL_REQUIRED_FLAG_BIT && pos.storage.size() != pos.max_words)) {
                        return ARG_PARSER_NO_REQUIRED_POSITIONAL;
                    }
                }

                curr_comm = curr_comm->parent_comm;
            }

            return ARG_PARSER_SUCCESS;
        }

        void print_help(const char* utility_name, command_ref* comm = nullptr) {

            command* command;
            if (comm) {
                command = &comm->m_ref;
            } else {
                command = &root_command;
            }

            std::printf("Usage: %s", utility_name);

            if (command != &root_command) {
                std::printf(" ... %s", command->name.data());
            }

            for (const auto& pos : command->positionals) {
                if (pos.max_words == 1) {
                    if ((pos.flags & POS_ALL_REQUIRED_FLAG_BIT) || (pos.flags & POS_AT_LEAST_ONE_FLAG_BIT)) {
                        std::printf(" <%s>", pos.name.data());
                    } else {
                        std::printf(" %s", pos.name.data());
                    }
                } else if (pos.max_words == SIZE_MAX) {
                    if (pos.flags & POS_AT_LEAST_ONE_FLAG_BIT) {
                        std::printf(" <%s...", pos.name.data());
                    } else {
                        std::printf(" %s...", pos.name.data());
                    }
                } else {
                    if (pos.flags & POS_ALL_REQUIRED_FLAG_BIT) {
                        std::printf(" <%s[%ld]>", pos.name.data(), pos.max_words);
                    } else if (pos.flags & POS_AT_LEAST_ONE_FLAG_BIT) {
                        std::printf(" <%s[%ld]", pos.name.data(), pos.max_words);
                    } else {
                        std::printf(" %s[%ld]", pos.name.data(), pos.max_words);
                    }
                }
            }

            if (!command->flags.empty()) {
                std::printf(" [options]");
            }

            if (!command->subcommands.empty()) {
                std::printf(" [subcommand] ...");
            }

            std::printf("\n");
        }

        void print_help_advanced(const char* utility_desc, const char* utility_name, command_ref* comm = nullptr) {

            std::printf("%s\n\n", utility_desc);

            command* command;
            if (comm) {
                command = &comm->m_ref;
            } else {
                command = &root_command;
            }

            print_help(utility_name, comm);

            if (!command->help.empty()) {
                std::printf("\n%s\n", command->help.data());
            }

            if (!command->flags.empty()) {
                std::printf("\nOptions:\n");

                bool all_longs = true;
                bool some_shorts_value = false;
                for (const auto& flag : command->flags) {
                    if (!flag.long_name.empty()) {
                        all_longs = false;
                    }
                    if (flag.short_name != 0) {
                        if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                            some_shorts_value = true;
                        }
                    }
                }

                size_t max_prefix_size = 0;
                for (const auto& flag : command->flags) {
                    size_t prefix_size = 2;
                    if (flag.short_name != 0) {
                        prefix_size += 2;
                        if (some_shorts_value) prefix_size += 1;
                    } else if (!all_longs) {
                        prefix_size += 2;
                        if (some_shorts_value) prefix_size += 1;
                    }
                    if (!flag.long_name.empty()) {
                        prefix_size += 3 + flag.long_name.size();
                        if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                            prefix_size += 1;
                        }
                    }
                    if (max_prefix_size < prefix_size) max_prefix_size = prefix_size;
                }

                for (const auto& flag : command->flags) {
                    size_t prefix_size = 2;
                    std::printf("  ");
                    if (flag.short_name != 0) {
                        std::printf("-%c", flag.short_name);
                        prefix_size += 2;
                        if (some_shorts_value) {
                            if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                                std::printf("=");
                            } else {
                                std::printf(" ");
                            }
                            prefix_size += 1;
                        }
                    } else if (!all_longs) {
                        std::printf("  ");
                        prefix_size += 2;
                        if (some_shorts_value) {
                            std::printf(" ");
                            prefix_size += 1;
                        }
                    }
                    if (!flag.long_name.empty()) {
                        std::printf(" --%s", flag.long_name.data());
                        prefix_size += 3 + flag.long_name.size();
                        if (flag.flags & FLAG_HAS_VALUE_FLAG_BIT) {
                            prefix_size += 1;
                            std::printf("=");
                        }
                    }
                    std::printf("%*c", static_cast<int>(max_prefix_size - prefix_size + 2), ' ');
                    if (!flag.help.empty()) {
                        std::printf("%s", flag.help.data());
                    }
                    std::printf("\n");
                }
            }

            if (!command->positionals.empty()) {
                std::printf("\nPositionals:\n");

                size_t max_prefix_size = 0;
                for (const auto& pos : command->positionals) {
                    size_t prefix_size = 2;
                    if (!pos.name.empty()) prefix_size += pos.name.size();
                    if (max_prefix_size < prefix_size) max_prefix_size = prefix_size;
                }

                for (const auto& pos : command->positionals) {
                    size_t prefix_size = 2;
                    std::printf("  ");
                    if (!pos.name.empty()) {
                        std::printf("%s", pos.name.data());
                        prefix_size += pos.name.size();
                    }
                    std::printf("%*c", static_cast<int>(max_prefix_size - prefix_size + 2), ' ');
                    if (!pos.help.empty()) {
                        std::printf("%s", pos.help.data());
                    }
                    std::printf("\n");
                }
            }

            if (!command->subcommands.empty()) {
                std::printf("\nSubcommands:\n");

                size_t max_prefix_size = 0;
                for (const auto& command : command->subcommands) {
                    size_t prefix_size = 2;
                    if (!command.name.empty()) prefix_size += command.name.size();
                    if (max_prefix_size < prefix_size) max_prefix_size = prefix_size;
                }

                for (const auto& command : command->subcommands) {
                    size_t prefix_size = 2;
                    std::printf("  ");
                    if (!command.name.empty()) {
                        std::printf("%s", command.name.data());
                        prefix_size += command.name.size();
                    }
                    std::printf("%*c", static_cast<int>(max_prefix_size - prefix_size + 2), ' ');
                    if (!command.help.empty()) {
                        std::printf("%s", command.help.data());
                    }
                    std::printf("\n");
                }
            }

        }

    private:

        command& get_command(command_ref* p_command) {
            if (p_command) return p_command->m_ref;
            else return root_command;
        }

        std::deque<std::string_view> parse_commas(const char* value) {
            std::deque<std::string_view> values;
            const char* str_end = std::strlen(value) + value;
            const char* end = value;
            const char* start = value;
            while (start < str_end) {
                end = std::strchr(start, ',');
                if (end) {
                    values.emplace_back(start, end - start);
                    start = end + 1;
                } else {
                    values.emplace_back(start, str_end - start);
                    start = str_end;
                }
            }
            return values;
        }

        command root_command;

};


#endif  // ARG_PARSER_HPP_
