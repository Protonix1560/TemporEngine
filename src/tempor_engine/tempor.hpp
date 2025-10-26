#pragma once

#include "window_manager.hpp"
#include "io.hpp"
#include "logger.hpp"
#include "gui_processor.hpp"
#include "gpu_backend.hpp"

#include <chrono>
#include <deque>
#include <algorithm>
#include <cstring>
#include <charconv>
#include <thread>

#include <vulkan/vulkan_core.h>




class WaitClock {

    public:
        double fps;
        double epsilon;
        size_t fpsSamplesCount;

        WaitClock(double fps = 60, double epsilon = -0.000054, size_t fpsSamplesCount = 10)
            : fps(fps), epsilon(epsilon), fpsSamplesCount(fpsSamplesCount) {}

    private:
        double prevTime;
        double time;
        
        std::deque<double> fpsHistory;
        double fpsSampleSum = 0.0;

        double deltaTime = 0.0;

        bool firstTick = true;

    
    public:

        double getDeltaTime() {
            return deltaTime;
        };

        double estimateFps() {
            if (fpsHistory.empty()) return fps;
            return fpsSampleSum / (double)fpsHistory.size();
        }

        void tick() {

            prevTime = time;

            time = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            double inversedFps = (fps > 0) ? 1.0 / fps : 0.0;
            double waitTime = inversedFps - time + prevTime + epsilon;

            auto start = std::chrono::high_resolution_clock::now();

            while (waitTime > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
                waitTime -= elapsed;
                start = now;
            }

            time = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            if (firstTick) {
                deltaTime = inversedFps;
                firstTick = false;
            } else {
                deltaTime = time - prevTime;
            }

            double inversedDeltaTime = (deltaTime != 0.0) ? 1.0 / deltaTime : 0.0;

            fpsSampleSum += inversedDeltaTime;
            fpsHistory.push_back(inversedDeltaTime);

            while (fpsHistory.size() > fpsSamplesCount) {
                fpsSampleSum -= fpsHistory.front();
                fpsHistory.erase(fpsHistory.begin());
            }
        }
};



class ArgParser {

    public:
        enum FlagParams : unsigned int {
            FLAG_HAS_VALUE = 0x1,
            FLAG_HELP_MSG = 0x2,
            FLAG_REQUIRED = 0x4,
        };

        enum PosParams : unsigned int {
            POS_REQUIRED = 0x1,
            POS_ALL_REMAINING = 0x2,
        };

        enum ErrCode : unsigned int {
            Success = 0,
            ErrValueMissing = 1,
            ErrUnknownFlag = 2,
            HelpCalled = 3,
            ErrInvalidValue = 4,
            ErrFlagMissing = 5,
            ErrPosMissing = 6,
        };

    private:
        struct Flag {
            char shortName;
            std::string longName;
            unsigned int params;
            std::string help;

            int count = 0;
            std::string storage;
        };

        struct Pos {
            std::string name;
            unsigned int params;
            std::string help;

            std::vector<std::string> storage;
        };

        template<typename T>
        static T evaluate(std::string& s, ErrCode* result) {

            if constexpr (std::is_same_v<T, std::string>) {
                return s;

            } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {

                T value{};
                auto first = s.data();
                auto last = s.data() + s.size();

                auto [ptr, ec] = std::from_chars(first, last, value, 10);
                if (ec == std::errc::invalid_argument) {
                    std::cerr << "Invalid number: " << s << "\n";
                    if (result) *result = ErrInvalidValue;
                } else if (ec == std::errc::result_out_of_range) {
                    std::cerr << "Too large number: " << s << "\n";
                    if (result) *result = ErrInvalidValue;
                }

                return value;
            
            } else if constexpr (std::is_same_v<T, float>) {
                return std::stof(s);

            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(s);

            } else if constexpr (std::is_same_v<T, long double>) {
                return std::stold(s);

            } else if constexpr (std::is_same_v<T, bool>) {
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                if (s == "true" || s == "t" || s == "yes" || s == "y" || s == "on" || s == "1") {
                    return true;
                } else if (s == "false" || s == "f" || s == "no" || s == "n" || s == "off" || s == "0") {
                    return false;
                } else {
                    std::cerr << "Invalid boolean: " << s << "\n";
                    if (result) *result = ErrInvalidValue;
                    return T{};
                }

            } else {
                static_assert(!sizeof(T), "Unsupported flag value type");
            }
        }


    public:

        struct FlagRef {

            public:

                template<typename T>
                T value(ErrCode* result = nullptr, T defVal = T{}) const {
                    if (!present()) return defVal;
                    return evaluate<T>(parser.flags[index].storage, result);
                }

                bool present() const { return parser.flags[index].count; }

                int count() const { return parser.flags[index].count; }

            private:
                int index;
                ArgParser& parser;

                FlagRef(size_t index, ArgParser& parser)
                    : index(index), parser(parser) {}

                friend class ArgParser;
        };

        FlagRef declareFlag(char shortName = 0, const char* longName = nullptr, unsigned int params = 0, const char* help = nullptr) {
            if (!longName) longName = "";
            if (!help) {
                if (params & FLAG_HELP_MSG) help = "Shows help message and exits";
                else help = "";
            }
            if ((params & FLAG_HELP_MSG) && (params & FLAG_HAS_VALUE)) {
                throw std::runtime_error("A flag can't both have a value and be a help messenger");
            }
            if ((params & FLAG_HELP_MSG) && (params & FLAG_REQUIRED)) {
                throw std::runtime_error("A flag can't both be required and be a help messenger");
            }
            if (longName[0] == 0 && shortName == 0) {
                throw std::runtime_error("A flag must have long or short name");
            }
            Flag flag{shortName, longName, params, help};
            flags.push_back(flag);
            FlagRef ref(flags.size() - 1, *this);
            return std::move(ref);
        }

        struct PosRef {

            public:
                bool present() const {
                    return parser.posArgs[index].storage.size() != 0;
                }

                int count() const {
                    return parser.posArgs[index].storage.size();
                }

                template<typename T>
                T value(int i = 0, ErrCode* result = nullptr, T defVal = T{}) const {
                    if (i < 0 || i > parser.posArgs[index].storage.size()) throw std::runtime_error("Out of range");
                    if (!present()) return defVal;
                    return evaluate<T>(parser.posArgs[index].storage[i], result);
                }

            private:
                int index;
                ArgParser& parser;

                PosRef(size_t index, ArgParser& parser)
                    : index(index), parser(parser) {}

                friend class ArgParser;
        };

        PosRef declarePositional(const char* name, unsigned int params = 0, const char* help = nullptr) {
            static int index = 0;
            static bool prevAllRem = false;
            if (prevAllRem && (params & POS_ALL_REMAINING)) {
                throw std::runtime_error("There can't be any positionals declared after a declaration of an all-remaining positional");
            }
            Pos pos{name, params, help};
            prevAllRem = params & POS_ALL_REMAINING;
            index++;
            posArgs.push_back(pos);
            PosRef ref(posArgs.size() - 1, *this);
            return std::move(ref);
        }

        void printHelp() {
            std::cout << "Usage: " << name;
            if (flags.size()) std::cout << " [options]";
            std::cout << " ";
            size_t maxPosLength = 0;
            for (const auto& pos : posArgs) {
                if (pos.params & POS_REQUIRED) std::cout << "<";
                else std::cout << "[";
                std::cout << pos.name;
                if (pos.params & POS_REQUIRED) std::cout << ">";
                else std::cout << "]";
                if (pos.params & POS_ALL_REMAINING) std::cout << "...";
                std::cout << " ";
                size_t len = 4 + pos.name.size() + ((pos.params & POS_ALL_REMAINING) ? 3 : 0);
                if (len > maxPosLength && len < maxHelpPadding) {
                    maxPosLength = len;
                }
            }
            std::cout << "\n";

            size_t maxFlagLength = 0;
            for (const auto& flag : flags) {
                size_t len = (flag.shortName ? 4 : 0) + (!flag.longName.empty() ? 3 : 0) + flag.longName.size() + 2;
                if (len > maxFlagLength && len < maxHelpPadding) {
                    maxFlagLength = len;
                }
            }

            if (flags.size()) {
                std::cout << "Options:\n";
                for (const auto& flag : flags) {
                    if (flag.shortName != 0) std::cout << "  -" << flag.shortName;
                    if (!flag.longName.empty()) std::cout << " --" << flag.longName;
                    std::cout << "  ";
                    size_t len = (flag.shortName ? 4 : 0) + (!flag.longName.empty() ? 3 : 0) + flag.longName.size() + 2;
                    if (len < maxFlagLength) {
                        std::cout << std::string(maxFlagLength - len, ' ');
                    }
                    std::string help = flag.help;
                    size_t nlinepos = help.find('\n');
                    std::cout << help.substr(0, nlinepos) << "\n";
                    help = help.substr(nlinepos + 1);
                    while (nlinepos != std::string::npos) {
                        nlinepos = help.find('\n');
                        std::cout << std::string(maxFlagLength, ' ') << help.substr(0, nlinepos) << "\n";
                        help = help.substr(nlinepos + 1);
                    }
                }
            }

            if (posArgs.size()) {
                std::cout << "\nArguments:\n";
                for (const auto& pos : posArgs) {
                    size_t len = 4 + pos.name.size() + ((pos.params & POS_ALL_REMAINING) ? 3 : 0);
                    if (pos.params & POS_REQUIRED) std::cout << "<";
                    else std::cout << "[";
                    std::cout << pos.name;
                    if (pos.params & POS_REQUIRED) std::cout << ">";
                    else std::cout << "]";
                    if (pos.params & POS_ALL_REMAINING) std::cout << "...";
                    std::cout << "  ";
                    if (len < maxPosLength) {
                        std::cout << std::string(maxPosLength - len, ' ');
                    }
                    std::string help = pos.help;
                    size_t nlinepos = help.find('\n');
                    std::cout << help.substr(0, nlinepos) << "\n";
                    help = help.substr(nlinepos + 1);
                    while (nlinepos != std::string::npos) {
                        nlinepos = help.find('\n');
                        std::cout << std::string(maxPosLength, ' ') << help.substr(0, nlinepos) << "\n";
                        help = help.substr(nlinepos + 1);
                    }
                }
            }

            if (!helpPost.empty()) std::cout << "\n" << helpPost << "\n";
        }

        ErrCode parse(int argc, char* argv[]) {

            bool forcePositional = false;
            int prevPos = 0;
            bool prevValueFlag = false;

            for (int i = 1; i < argc; i++) {

                if (std::strlen(argv[i]) == 2 && argv[i][0] == '-' && argv[i][1] == '-') {
                    forcePositional = true;

                } else if (std::strlen(argv[i]) > 2 && argv[i][0] == '-' && argv[i][1] == '-' && !forcePositional) {
                    auto it = std::find_if(flags.begin(), flags.end(), [&argv, i = i](const Flag& a) -> bool {
                        return std::strncmp(a.longName.c_str(), argv[i] + 2, a.longName.size()) == 0 && (argv[i][2 + a.longName.size()] == 0 || argv[i][2 + a.longName.size()] == '=');
                    });
                    if (it != flags.end()) {
                        it->count++;
                        if (it->params & FLAG_HAS_VALUE) {
                            prevValueFlag = true;
                            if (it->longName.size() != std::strlen(argv[i] + 2) && argv[i][2 + it->longName.size()] == '=') {
                                it->storage.assign(argv[i] + 3 + it->longName.size());
                            } else if (argc > i + 1) {
                                it->storage.assign(argv[i + 1]);
                            } else {
                                std::cerr << "Flag --" << it->longName << " needs a value but nothing is specified\n";
                                return ErrValueMissing;
                            }
                        }
                        if (it->params & FLAG_HELP_MSG) {
                            printHelp();
                            return HelpCalled;
                        }
                    } else {
                        std::cerr << "Unknown flag " << argv[i] << "\n";
                        return ErrUnknownFlag;
                    }

                } else if (std::strlen(argv[i]) > 1 && argv[i][0] == '-' && !forcePositional) {
                    for (int j = 1; j < std::strlen(argv[i]); j++) {
                        auto it = std::find_if(flags.begin(), flags.end(), [&argv, i = i, j = j](const Flag& a) -> bool {
                            return argv[i][j] == a.shortName;
                        });
                        if (it != flags.end()) {
                            it->count++;
                            if (it->params & FLAG_HAS_VALUE) {
                                prevValueFlag = true;
                                if (std::strlen(argv[i]) > 2) {
                                    it->storage.assign(argv[i] + 2);
                                    break;
                                } else if (argc > i + 1) {
                                    it->storage.assign(argv[i + 1]);
                                } else {
                                    std::cerr << "Flag -" << it->shortName << " needs a value but nothing is specified\n";
                                    return ErrValueMissing;
                                }
                            }
                            if (it->params & FLAG_HELP_MSG) {
                                printHelp();
                                return HelpCalled;
                            }
                        } else {
                            std::cerr << "Unknown flag -" << argv[i][j] << "\n";
                            return ErrUnknownFlag;
                        }
                    }

                } else {
                    if (!prevValueFlag) {
                        posArgs[prevPos].storage.push_back(argv[i]);
                        if ((posArgs[prevPos].params & POS_ALL_REMAINING) == 0) prevPos++;
                    }
                    prevValueFlag = false;
                }

            }

            for (const auto& flag : flags) {
                if (flag.count == 0 && (flag.params & FLAG_REQUIRED)) {
                    std::cerr << "Required flag";
                    if (!flag.longName.empty()) std::cerr << " --" << flag.longName;
                    else if (flag.shortName != 0) std::cerr << " -" << flag.shortName;
                    std::cerr << " is missing\n";
                    return ErrFlagMissing;
                }
            }

            for (const auto& pos : posArgs) {
                if (!pos.storage.size() && (pos.params & POS_REQUIRED)) {
                    std::cerr << "Required positional argument <" << pos.name << "> is missing\n";
                    return ErrPosMissing;
                }
            }

            return Success;
        }

        ArgParser(const char* utilityName = nullptr, const char* helpPostfix = nullptr, size_t maxHelpPadding = 32)
            : maxHelpPadding(maxHelpPadding) {

            if (!utilityName) utilityName = "";
            name.assign(utilityName);
            if (!helpPostfix) helpPostfix = "";
            helpPost.assign(helpPostfix);
        }

    private:
        std::vector<Flag> flags;
        std::string name;
        std::string helpPost;
        size_t maxHelpPadding;
        std::vector<Pos> posArgs;

};




class TemporEngine {

    public:
        int run(int argc, char* argv[]) noexcept;

    private:
        std::unique_ptr<WindowManager> mWindowManager;
        std::unique_ptr<Backend> mBackend;
        WaitClock mClock{6000000};
        IOManager mIO;
        GlobalServiceLocator mServiceLocator;
        Logger mLogger;
        GUIProcessor mGUIProcessor;
        Settings mSettings;

        inline void initialize(int verboseLevel);
        inline void mainloop();
        inline void shutdown() noexcept;

};



