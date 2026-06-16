
#include "settings.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "resource_registry.hpp"
#include "logger.hpp"

#include <exception>
#include <string_view>
#include <variant>

#include <simdjson.h>

namespace sj = simdjson;


Settings::Settings(Logger logger, ResourceRegistry& rResReg, std::filesystem::path confPath, bool flushConfig)
    : mLogger(logger), mrResReg(rResReg), mFlushConfig(flushConfig) {

    if (confPath.empty()) {
        confPath = "config.json";
    }

    auto confExp = mrResReg.matchFile(confPath);
    if (!confExp.has_value()) return;
    mConfPath = confExp.value();

    mLogger.debug() << "Found config file at \"" << mConfPath.string() << "\"\n";

    auto openExp = mrResReg.openResource(mConfPath);
    if (!openExp.has_value()) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to open config file [" << openExp.error() << "]\n";
        return;
    }
    TprResource confRes = openExp.value();

    auto dataExp = mrResReg.getResourceConstPointer(confRes);
    auto sizeExp = mrResReg.sizeofResource(confRes);
    if (dataExp.has_value() && sizeExp.has_value()) {
        const std::byte* data = dataExp.value();
        size_t size = sizeExp.value();
        mJsonData.emplace();
        mJsonData->data = sj::padded_string(reinterpret_cast<const char*>(data), size);
    }
    mrResReg.closeResource(confRes);

    if (mJsonData.has_value()) {
        auto docExp = mJsonData->parser.parse(mJsonData->data);
        if (!docExp.has_value()) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to parse config file: " << docExp.error() << "\n";
            mJsonData.reset();
        } else {
            mJsonData->root = docExp.value();
        }
    }
}

Settings::~Settings() {
    flush();
}


expected<TprSetting, TprResult> Settings::createSetting(std::string_view name) noexcept {

    TprSetting handle;

    try {
        auto it = std::ranges::find_if(mSettings, [name](const decltype(mSettings)::value_type& value) -> bool {
            const auto& setting = value.second;
            return setting.name == name;
        });
        if (it != mSettings.end()) {
            auto& setting = it->second;
            setting.refCount++;
            mSettingHandles.try_emplace(mHandleCount, it->first);
            handle = construct_basic_handle<TprSetting>(mHandleCount, 0, handle_type::setting);
            mHandleCount++;

        } else {
            auto& setting = mSettings.try_emplace(mSettingCount, name).first->second;

            if (mJsonData.has_value()) {
                auto el = mJsonData->root.at_key(setting.name);
                if (el.has_value()) {
                    if (el->is_bool()) {
                        auto dataExp = el.get_bool();
                        if (dataExp.has_value()) {
                            setting.data = dataExp.value();
                        }
                    } else if (el->is_string()) {
                        auto dataExp = el.get_string();
                        if (dataExp.has_value()) {
                            setting.data = std::string(dataExp.value());
                        }
                    } else if (el->is_int64()) {
                        auto dataExp = el.get_int64();
                        if (dataExp.has_value()) {
                            setting.data = dataExp.value();
                        }
                    } else if (el.is_double()) {
                        auto dataExp = el.get_double();
                        if (dataExp.has_value()) {
                            setting.data = dataExp.value();
                        }
                    } else if (el.is_null()) {
                        setting.data = std::nullptr_t{};
                    }
                } else {
                    setting.data = std::monostate{};
                }
            }

            mSettingHandles.try_emplace(mHandleCount, mSettingCount);

            handle = construct_basic_handle<TprSetting>(mHandleCount, 0, handle_type::setting);
            mHandleCount++;
            mSettingCount++;

            if (mJsonData.has_value()) {
                auto l = mLogger.debug();
                l << "Read setting " << name;
                std::visit(overload{
                    [&l](const double& value) { l << " = " << value; },
                    [&l](const int64_t& value) { l << " = " << value; },
                    [&l](const std::nullptr_t& value) { l << " = null"; },
                    [&l](const std::monostate& value) { l << " <unset>"; },
                    [&l](const bool& value) { l << (value ? " = true" : " = false"); },
                    [&l](const std::string& value) { l << " = " << value; }
                }, setting.data);
                l << "\n";
            } else {
                mLogger.debug() << "Created setting " << name << "\n";
            }
        }

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return unexpected(TPR_PANIC);
    }

    return handle;
}


void Settings::destroySetting(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return;
        auto& setting = mSettings.at(it->second.key);
        setting.refCount--;
        if (setting.refCount == 0) {
            mSettings.erase(mSettings.find(it->second.key));
        }
        mSettingHandles.erase(it);

    } catch (...) {}
}


expected<TprSettingType, TprResult> Settings::getSettingType(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& setting = mSettings.at(it->second.key);
        return std::visit(overload{
            [](const double& value) { return TPR_SETTING_TYPE_FLOATING; },
            [](const int64_t& value) { return TPR_SETTING_TYPE_INTEGER; },
            [](const std::monostate& value) { return TPR_SETTING_TYPE_UNSET; },
            [](const std::nullptr_t& value) { return TPR_SETTING_TYPE_NULL; },
            [](const bool& value) { return TPR_SETTING_TYPE_BOOL; },
            [](const std::string& value) { return TPR_SETTING_TYPE_STRING; }
        }, setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<double, TprResult> Settings::getSettingDouble(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<double>(setting.data)) return unexpected(TPR_WRONG_TYPE);
        return std::get<double>(setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<int64_t, TprResult> Settings::getSettingInteger(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<int64_t>(setting.data)) return unexpected(TPR_WRONG_TYPE);
        return std::get<int64_t>(setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<TprBool8, TprResult> Settings::getSettingBool(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<bool>(setting.data)) return unexpected(TPR_WRONG_TYPE);
        return std::get<bool>(setting.data);
    } catch (...) {
        return unexpected(TPR_PANIC);
    }
}

expected<uint32_t, TprResult> Settings::getSettingStringSize(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<std::string>(setting.data)) return unexpected(TPR_WRONG_TYPE);
        return std::get<std::string>(setting.data).size() + 1;  // to include null-terminator
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

TprResult Settings::copySettingString(TprSetting setting, char* pData) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<std::string>(setting.data)) return TPR_WRONG_TYPE;
        auto& s = std::get<std::string>(setting.data);
        std::strncpy(pData, s.data(), s.size());
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingDouble(TprSetting setting, double data) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = data;
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingInteger(TprSetting setting, int64_t data) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = data;
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingBool(TprSetting setting, TprBool8 data) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = data;
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingString(TprSetting setting, const char* pData) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = std::string(pData);
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingNull(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = std::nullptr_t{};
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprResult Settings::unsetSetting(TprSetting setting) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& setting = mSettings.at(it->second.key);
        setting.data = std::monostate{};
        flush();
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}


double Settings::getSettingDoubleOr(TprSetting setting, double fallback) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<double>(setting.data)) return fallback;
        return std::get<double>(setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

int64_t Settings::getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<int64_t>(setting.data)) return fallback;
        return std::get<int64_t>(setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}

TprBool8 Settings::getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept {
    try {
        if (get_basic_handle_index(setting) > mHandleCount) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto& setting = mSettings.at(it->second.key);
        if (!std::holds_alternative<bool>(setting.data)) return fallback;
        return std::get<bool>(setting.data);
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1) << "Unknown exception\n";
        return TPR_PANIC;
    }
}


void Settings::flush() {

    if (!mFlushConfig) return;

    auto confExp = mrResReg.matchFile(mConfPath);
    auto confPath = confExp.has_value() ? confExp.value() : "config.json";

    mLogger.debug() << "Writing config file to \"" << confPath.string() << "\"\n";

    auto openExp = mrResReg.openResource(confPath, TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT | TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT);
    if (!openExp.has_value()) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to open config file [" << openExp.error() << "]\n";
    }
    TprResource confRes = openExp.value();

    std::string dataString;
    dataString += "{\n";
    for (auto it = mSettings.cbegin(); it != mSettings.cend(); it++) {
        if (it != mSettings.cend() && it != mSettings.cbegin()) {
            dataString += ",\n";
        }
        const auto& setting = it->second;
        if (!std::holds_alternative<std::monostate>(setting.data)) {
            dataString += "    \"" + setting.name + "\": ";
            dataString += std::visit(overload{
                [](const std::string& value) {
                    return "\""s + value + "\""s;
                },
                [](const std::nullptr_t& value) {
                    return "null"s;
                },
                [](const std::monostate& value) {
                    return "\"<unset>\""s;
                },
                [](const bool& value) {
                    return value ? "true"s : "false"s;
                },
                [](const auto& value) {
                    return std::to_string(value);
                }
            }, setting.data);
        }
    }
    dataString += "\n}";

    TprResult resizeRes = mrResReg.resizeResource(confRes, dataString.size());
    auto dataExp = mrResReg.getResourceRawDataPointer(confRes);
    if (resizeRes == TPR_SUCCESS && dataExp.has_value()) {
        std::byte* data = dataExp.value();
        std::memcpy(data, dataString.data(), dataString.size());
    } else {
        mrResReg.closeResource(confRes);
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to write to config file [" << dataExp.error() << "]\n";
    }
    mrResReg.closeResource(confRes);
}


void Settings::finalizeRead() {
    mLogger.debug() << "Finalizing read from config file\n";
    mJsonData.reset();
}

double Settings::createSettingDoubleOr(std::string_view name, double fallback) noexcept {
    auto createExp = createSetting(name);
    if (!createExp.has_value()) return fallback;
    return getSettingDoubleOr(createExp.value(), fallback);
}


int64_t Settings::createSettingIntegerOr(std::string_view name, int64_t fallback) noexcept {
    auto createExp = createSetting(name);
    if (!createExp.has_value()) return fallback;
    return getSettingIntegerOr(createExp.value(), fallback);
}


TprBool8 Settings::createSettingBoolOr(std::string_view name, TprBool8 fallback) noexcept {
    auto createExp = createSetting(name);
    if (!createExp.has_value()) return fallback;
    return getSettingBoolOr(createExp.value(), fallback);
}


std::string Settings::createSettingStringOr(std::string_view name, std::string fallback) noexcept {
    auto createExp = createSetting(name);
    if (!createExp.has_value()) return fallback;
    auto setting = createExp.value();
    auto sizeExp = getSettingStringSize(setting);
    if (!sizeExp.has_value()) return fallback;
    auto size = sizeExp.value();
    std::string string(size, '\0');
    auto copyRes = copySettingString(setting, string.data());
    if (copyRes != TPR_SUCCESS) return fallback;
    return string;
}


