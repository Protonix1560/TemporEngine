
#ifndef SETTINGS_SETTINGS_HPP_
#define SETTINGS_SETTINGS_HPP_


#include "plugin_core.h"
#include "core.hpp"
#include "logger.hpp"

#include <optional>
#include <variant>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <mutex>

#include <simdjson.h>

namespace sj = simdjson;


// from "file_registry.hpp"
class FileRegistry;


struct SettingUnset {};
struct SettingNull {};
struct SettingString { std::string value; };
struct SettingInt { int64_t value; };
struct SettingDouble { double value; };
struct SettingBool { bool value; };
struct SettingArray { std::vector<uint32_t> elements; };
struct SettingStruct { std::vector<uint32_t> elements; };

struct Setting {
    std::string name;
    std::variant<SettingUnset, SettingNull, SettingString, SettingInt, SettingBool, SettingDouble, SettingArray, SettingStruct> data = SettingUnset{};
    size_t refCount = 1;
    std::optional<sj::dom::element> element;
};


struct SettingHandle {
    uint32_t setting;
    TprSettingCapabilityFlags capability;
};


struct JsonData {
    sj::padded_string data;
    sj::dom::parser parser;
    sj::dom::element root;
};


class Settings {

    public:
        Settings(Logger logger, FileRegistry& rResReg);
        TprResult init(std::filesystem::path configPath, bool flushConfig, bool configEnabled);
        ~Settings();

        expected<TprSetting, TprResult> createSetting(TprSetting baseSetting, std::string_view name) noexcept;
        expected<TprSetting, TprResult> readSetting(TprSetting baseSetting, std::string_view name) noexcept;
        void destroySetting(TprSetting setting) noexcept;

        expected<TprSettingType, TprResult> getSettingType(TprSetting setting) noexcept;
        expected<double, TprResult> getSettingDouble(TprSetting setting) noexcept;
        expected<int64_t, TprResult> getSettingInteger(TprSetting setting) noexcept;
        expected<TprBool8, TprResult> getSettingBool(TprSetting setting) noexcept;

        expected<uint32_t, TprResult> getSettingStringSize(TprSetting setting) noexcept;
        TprResult copySettingString(TprSetting setting, char* pData) noexcept;

        TprResult setSettingDouble(TprSetting setting, double data) noexcept;
        TprResult setSettingInteger(TprSetting setting, int64_t data) noexcept;
        TprResult setSettingBool(TprSetting setting, TprBool8 data) noexcept;
        TprResult setSettingString(TprSetting setting, const char* pData) noexcept;
        TprResult setSettingNull(TprSetting setting) noexcept;
        TprResult unsetSetting(TprSetting setting) noexcept;
        TprResult setSettingStruct(TprSetting setting) noexcept;
        TprResult setSettingArray(TprSetting setting) noexcept;

        double getSettingDoubleOr(TprSetting setting, double fallback) noexcept;
        int64_t getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept;
        TprBool8 getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept;

        double createSettingDoubleOr(TprSetting baseSetting, std::string_view name, double fallback) noexcept;
        int64_t createSettingIntegerOr(TprSetting baseSetting, std::string_view name, int64_t fallback) noexcept;
        TprBool8 createSettingBoolOr(TprSetting baseSetting, std::string_view name, TprBool8 fallback) noexcept;

        std::string createSettingStringOr(TprSetting baseSetting, std::string_view name, std::string fallback) noexcept;

        expected<uint32_t, TprResult> getSettingArraySize(TprSetting setting) noexcept;
        expected<TprSetting, TprResult> getSettingArrayElement(TprSetting setting, uint32_t index) noexcept;
        TprResult resizeSettingArray(TprSetting setting, uint32_t size) noexcept;

        TprSetting getRoot() const;

        void finalizeRead();
        TprResult flush();

    private:

        TprResult openConfig();

        Setting parseSetting(sj::dom::element element);

        std::pair<
            std::unordered_map<uint32_t, Setting>::iterator,
            std::unordered_map<uint32_t, SettingHandle>::iterator
        > createNamedSetting(std::string_view name, sj::dom::element element);

        std::pair<
            std::unordered_map<uint32_t, Setting>::iterator,
            std::unordered_map<uint32_t, SettingHandle>::iterator
        > createUnnamedSetting(sj::dom::element element);

        void printSettingValue(LogEntry& log, const Setting& setting);
        void writeSetting(std::ostringstream& stream, size_t identation, const Setting& setting);

        void destroySettingById(uint32_t id);

        FileRegistry& mrFileReg;
        Logger mLogger;

        std::mutex mMutex;

        std::filesystem::path mConfPath;
        bool mFlushConfig;

        std::optional<JsonData> mJsonData;

        std::unordered_map<uint32_t, SettingHandle> mSettingHandles;
        std::unordered_map<uint32_t, Setting> mSettings;
        uint32_t mSettingCounter = 0;
        uint32_t mHandleCounter = 0;

        TprSetting mRootSetting;

};

REGISTER_TYPE_NAME_S(Settings, "Sett");


#endif  // SETTINGS_SETTINGS_HPP_
