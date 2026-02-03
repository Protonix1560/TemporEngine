

#ifndef TEMPOR_ENGINE_TEMPOR_HPP_
#define TEMPOR_ENGINE_TEMPOR_HPP_


#include "core.hpp"
#include "data_bridge.hpp"
#include "resource_registry.hpp"
#include "window_manager.hpp"
#include "logger.hpp"
#include "gui_processor.hpp"
#include "irenderer.hpp"
#include "plugin_launcher.hpp"
#include "scene_manager.hpp"
#include "asset_store.hpp"

#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <charconv>
#include <thread>
#include <csignal>




class SleepClock {

    public:

        SleepClock(
            double fps = 60.0, size_t timeSampleCount = 20, double errorGain = 0.08,
            double correctionMin = 0.0002, double correctionMax = 1.0, double epsilon = 0.0000026
        ) : mErrorGain(errorGain), mCorrectionMin(correctionMin), mCorrectionMax(correctionMax) {
                if (timeSampleCount == 0) throw Exception(ErrCode::InternalError, "timeSampleCount must be greater or equal to 1");
                setFps(fps);
                setTimeSampleCount(timeSampleCount);
                setEpsilon(epsilon);
                setCorrectionMin(correctionMin);
                setCorrectionMax(correctionMax);
                mPrevTime = std::chrono::steady_clock::now();
            }



        void setFps(double fps) {
            mFps = fps;
            mDesiredDeltaTime = std::chrono::duration<double>((mFps != 0) ? 1.0 / mFps : 0.0);
            mTimeSamples.clear();
        }

        void setErrorGain(double errorGain) {
            mErrorGain = errorGain;
        }

        void setTimeSampleCount(size_t timeSampleCount) {
            mTimeSampleCount = timeSampleCount;
            mRecordedTimeSampleCount = 0;
            mTimeSampleWalker = 0;
            mTimeSampleSum = std::chrono::duration<double>(0.0);
            mTimeSamples.assign(mTimeSampleCount, std::chrono::duration<double>(0.0));
        }

        void setEpsilon(double epsilon) {
            mEpsilon = std::chrono::duration<double>(epsilon);
        }

        void setCorrectionMin(double correctionMin) {
            mCorrectionMin = std::chrono::duration<double>(correctionMin);
        }

        void setCorrectionMax(double correctionMax) {
            mCorrectionMax = std::chrono::duration<double>(correctionMax);
        }



        void begin() {
            mPrevTime = std::chrono::steady_clock::now();
            mTickedTime = std::chrono::steady_clock::now();
        }

        void tick() {
            using namespace std::chrono;

            mCurrTime = steady_clock::now();
            mDeltaTime = mCurrTime - mPrevTime;
            duration<double> sleepTime = mDesiredDeltaTime - mDeltaTime - mEpsilon;
            
            std::this_thread::sleep_for(sleepTime - mSleepCorrection);

            // correcting sleep_for
            auto wake = mCurrTime + sleepTime;
            while (steady_clock::now() < wake) {
                std::this_thread::yield();
            }

            duration<double> totalTime = steady_clock::now() - mPrevTime;

            mPrevTime = steady_clock::now();

            mTimeSampleSum += totalTime;
            mTimeSampleSum -= mTimeSamples[mTimeSampleWalker];
            mTimeSamples[mTimeSampleWalker] = totalTime;

            mTimeSampleWalker++;
            if (mTimeSampleWalker == mTimeSampleCount) {
                mTimeSampleWalker = 0;
            }
            if (mRecordedTimeSampleCount < mTimeSampleCount) {
                mRecordedTimeSampleCount++;
            }

            duration<double> error = duration<double>(estimateDeltaTime()) - mDesiredDeltaTime;
            mSleepCorrection += error * mErrorGain;
            mSleepCorrection = std::clamp(mSleepCorrection, mCorrectionMin, mCorrectionMax);
        }

        double estimateFps() {
            if (mRecordedTimeSampleCount == 0) return mFps;
            return 1.0 / (mTimeSampleSum / mRecordedTimeSampleCount).count();
        }

        double estimateDeltaTime() {
            if (mRecordedTimeSampleCount == 0) return mDesiredDeltaTime.count();
            return (mTimeSampleSum / mRecordedTimeSampleCount).count();
        }

        size_t ticked() {
            using namespace std::chrono;

            auto currTime = steady_clock::now();
            size_t ticks = static_cast<size_t>((currTime - mTickedTime) / mDesiredDeltaTime);
            if (ticks > 0) mTickedTime = steady_clock::now();

            return ticks;
        }


    private:
        double mFps;
        std::chrono::duration<double> mDesiredDeltaTime;

        std::chrono::time_point<std::chrono::steady_clock> mPrevTime;
        std::chrono::time_point<std::chrono::steady_clock> mCurrTime;
        std::chrono::duration<double> mDeltaTime;
        std::chrono::time_point<std::chrono::steady_clock> mTickedTime;

        size_t mTimeSampleCount;
        size_t mTimeSampleWalker;
        size_t mRecordedTimeSampleCount;
        std::vector<std::chrono::duration<double>> mTimeSamples;
        std::chrono::duration<double> mTimeSampleSum{0.0};

        double mErrorGain;
        std::chrono::duration<double> mSleepCorrection{0.0};
        std::chrono::duration<double> mCorrectionMin;
        std::chrono::duration<double> mCorrectionMax;
        std::chrono::duration<double> mEpsilon;

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

            auto& log = gGetServiceLocator()->get<Logger>();

            if constexpr (std::is_same_v<T, std::string>) {
                return s;

            } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {

                T value{};
                auto first = s.data();
                auto last = s.data() + s.size();

                auto [ptr, ec] = std::from_chars(first, last, value, 10);
                if (ec == std::errc::invalid_argument) {
                    log.error(TPR_LOG_STYLE_ERROR1) << "Invalid number: " << s << "\n";
                    if (result) *result = ErrInvalidValue;
                } else if (ec == std::errc::result_out_of_range) {
                    log.error(TPR_LOG_STYLE_ERROR1) << "Too large number: " << s << "\n";
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
                    log.error(TPR_LOG_STYLE_ERROR1) << "Invalid boolean: " << s << "\n";
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
            auto& log = gGetServiceLocator()->get<Logger>();
            auto l = log.info(TPR_LOG_STYLE_STANDART);
            l << "Usage: " << name;
            if (flags.size()) l << " [options]";
            l << " ";
            size_t maxPosLength = 0;
            for (const auto& pos : posArgs) {
                if (pos.params & POS_REQUIRED) l << "<";
                else l << "[";
                l << pos.name;
                if (pos.params & POS_REQUIRED) l << ">";
                else l << "]";
                if (pos.params & POS_ALL_REMAINING) l << "...";
                l << " ";
                size_t len = 4 + pos.name.size() + ((pos.params & POS_ALL_REMAINING) ? 3 : 0);
                if (len > maxPosLength && len < maxHelpPadding) {
                    maxPosLength = len;
                }
            }
            l << "\n";

            size_t maxFlagLength = 0;
            for (const auto& flag : flags) {
                size_t len = (flag.shortName ? 4 : 0) + (!flag.longName.empty() ? 3 : 0) + flag.longName.size() + 2;
                if (len > maxFlagLength && len < maxHelpPadding) {
                    maxFlagLength = len;
                }
            }

            if (flags.size()) {
                l << "Options:\n";
                for (const auto& flag : flags) {
                    if (flag.shortName != 0) l << "  -" << flag.shortName;
                    if (!flag.longName.empty()) l << " --" << flag.longName;
                    l << "  ";
                    size_t len = (flag.shortName ? 4 : 0) + (!flag.longName.empty() ? 3 : 0) + flag.longName.size() + 2;
                    if (len < maxFlagLength) {
                        l << std::string(maxFlagLength - len, ' ');
                    }
                    std::string help = flag.help;
                    size_t nlinepos = help.find('\n');
                    l << help.substr(0, nlinepos) << "\n";
                    help = help.substr(nlinepos + 1);
                    while (nlinepos != std::string::npos) {
                        nlinepos = help.find('\n');
                        l << std::string(maxFlagLength, ' ') << help.substr(0, nlinepos) << "\n";
                        help = help.substr(nlinepos + 1);
                    }
                }
            }

            if (posArgs.size()) {
                l << "\nArguments:\n";
                for (const auto& pos : posArgs) {
                    size_t len = 4 + pos.name.size() + ((pos.params & POS_ALL_REMAINING) ? 3 : 0);
                    if (pos.params & POS_REQUIRED) l << "<";
                    else l << "[";
                    l << pos.name;
                    if (pos.params & POS_REQUIRED) l << ">";
                    else l << "]";
                    if (pos.params & POS_ALL_REMAINING) l << "...";
                    l << "  ";
                    if (len < maxPosLength) {
                        l << std::string(maxPosLength - len, ' ');
                    }
                    std::string help = pos.help;
                    size_t nlinepos = help.find('\n');
                    l << help.substr(0, nlinepos) << "\n";
                    help = help.substr(nlinepos + 1);
                    while (nlinepos != std::string::npos) {
                        nlinepos = help.find('\n');
                        l << std::string(maxPosLength, ' ') << help.substr(0, nlinepos) << "\n";
                        help = help.substr(nlinepos + 1);
                    }
                }
            }

            if (!helpPost.empty()) l << "\n" << helpPost << "\n";
        }

        ErrCode parse(int argc, char* argv[]) {

            auto& log = gGetServiceLocator()->get<Logger>();

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
                                log.error(TPR_LOG_STYLE_ERROR1) << "Flag --" << it->longName << " needs a value but nothing is specified\n";
                                return ErrValueMissing;
                            }
                        }
                        if (it->params & FLAG_HELP_MSG) {
                            printHelp();
                            return HelpCalled;
                        }
                    } else {
                        log.error(TPR_LOG_STYLE_ERROR1) << "Unknown flag " << argv[i] << "\n";
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
                                    log.error(TPR_LOG_STYLE_ERROR1) << "Flag -" << it->shortName << " needs a value but nothing is specified\n";
                                    return ErrValueMissing;
                                }
                            }
                            if (it->params & FLAG_HELP_MSG) {
                                printHelp();
                                return HelpCalled;
                            }
                        } else {
                            log.error(TPR_LOG_STYLE_ERROR1) << "Unknown flag -" << argv[i][j] << "\n";
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
                    log.error(TPR_LOG_STYLE_ERROR1) << "Required flag";
                    if (!flag.longName.empty()) log.error(TPR_LOG_STYLE_ERROR1) << " --" << flag.longName;
                    else if (flag.shortName != 0) log.error(TPR_LOG_STYLE_ERROR1) << " -" << flag.shortName;
                    log.error(TPR_LOG_STYLE_ERROR1) << " is missing\n";
                    return ErrFlagMissing;
                }
            }

            for (const auto& pos : posArgs) {
                if (!pos.storage.size() && (pos.params & POS_REQUIRED)) {
                    log.error(TPR_LOG_STYLE_ERROR1) << "Required positional argument <" << pos.name << "> is missing\n";
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
        SIG_SAFE void sigint() noexcept;
        SIG_SAFE void sigterm() noexcept;

    private:
        SleepClock mClock;

        std::unique_ptr<IRenderer> mpRenderer;
        WindowManager mWindowManager;
        Logger mLogger;
        PluginLauncher mPluginLauncher;
        TprEngineAPI mApi{};
        SceneManager mSceneManager;
        ResourceRegistry mResourceRegistry;
        AssetStore mAssetStore;

        // might be obsolete soon due to ResourceRegistry being better
        DataBridge mDataBridge;

        // are obsolete
        GUIProcessor mGUIProcessor;
        Settings mSettings;

        inline void init(int verboseLevel);
        inline void mainloop();
        inline void shutdown() noexcept;

        bool shouldStop = false;

        volatile sig_atomic_t mSigInt = 0;
        volatile sig_atomic_t mSigTerm = 0;

};



#endif  // TEMPOR_ENGINE_TEMPOR_HPP_
