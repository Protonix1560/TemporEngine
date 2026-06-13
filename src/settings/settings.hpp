
#ifndef SETTINGS_SETTINGS_HPP_
#define SETTINGS_SETTINGS_HPP_


#include "plugin_core.h"
#include "core.hpp"

#include <optional>
#include <variant>
#include <string_view>
#include <unordered_map>

#include <simdjson.h>

namespace sj = simdjson;


// from "resource_registry.hpp"
class ResourceRegistry;

// from "logger.hpp"
class Logger;


struct Setting {
    std::string name;
    std::variant<std::monostate, std::nullptr_t, std::string, int64_t, bool, double> data = std::monostate{};
    size_t refCount = 1;
    Setting(std::string_view n) : name(n) {}
};


struct SettingHandle {
    uint32_t key;
};


struct JsonData {
    sj::padded_string data;
    sj::dom::parser parser;
    sj::dom::element root;
};


class Settings {

    public:
        Settings(Logger& rLogger, ResourceRegistry& rResReg);
        ~Settings();

        expected<TprSetting, TprResult> createSetting(std::string_view name) noexcept;
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

        double getSettingDoubleOr(TprSetting setting, double fallback) noexcept;
        int64_t getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept;
        TprBool8 getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept;

        double createSettingDoubleOr(std::string_view name, double fallback) noexcept;
        int64_t createSettingIntegerOr(std::string_view name, int64_t fallback) noexcept;
        TprBool8 createSettingBoolOr(std::string_view name, TprBool8 fallback) noexcept;

        std::string createSettingStringOr(std::string_view name, std::string fallback) noexcept;

        void finalizeRead();
        TprResult sync();

    private:
        ResourceRegistry& mrResReg;
        Logger& mrLogger;

        std::optional<JsonData> mJsonData;

        std::unordered_map<uint32_t, SettingHandle> mSettingHandles;
        std::unordered_map<uint32_t, Setting> mSettings;
        uint32_t mSettingCount = 0;
        uint32_t mHandleCount = 0;

};

REGISTER_TYPE_NAME_S(Settings, "Sett");


#endif  // SETTINGS_SETTINGS_HPP_
