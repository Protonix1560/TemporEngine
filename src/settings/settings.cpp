
#include "settings.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "file_registry.hpp"
#include "logger.hpp"
#include "log_entry.hpp"

#include <exception>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

#include <simdjson.h>

namespace sj = simdjson;


Settings::Settings(Logger logger, FileRegistry& rFileReg, std::atomic<TprResult>& rRunResult) : mLogger(logger), mrFileReg(rFileReg), mrRunResult(rRunResult) {}

TprResult Settings::init(std::filesystem::path confPath, bool flushConfig, bool configEnabled) {

    mFlushConfig = flushConfig;
    mConfPath = confPath;
    if (mConfPath.empty()) {
        mConfPath = "config.json";
    }

    if (configEnabled) {
        auto res = openConfig();
        if (res != TPR_SUCCESS) return res;
    }

    // root setting
    Setting setting{};
    setting.name = "root";
    if (mJsonData.has_value()) {
        setting.element = mJsonData->root;
    }
    setting.data = SettingStruct{};
    SettingHandle handle{};
    handle.setting = mSettingCounter;
    handle.capability = TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT;
    mSettings.try_emplace(mSettingCounter, setting);
    mSettingHandles.try_emplace(mHandleCounter, handle);
    mRootSetting = construct_basic_handle<TprSetting>(mHandleCounter, 0, handle_type::setting);
    mSettingCounter++;
    mHandleCounter++;

    return TPR_SUCCESS;
}

TprResult Settings::openConfig() {
    auto openExp = mrFileReg.openFile(mConfPath);
    if (!openExp.has_value()) {
        mLogger.error() << "Failed to open config file [" << openExp.error() << "]";
        return TPR_SUCCESS;
    }
    TprFile config = openExp.value();
    scope_exit closeConfig([&]() { mrFileReg.closeFile(config); });

    TprResult result;
    result = mrFileReg.seek(config, 0, TPR_SEEK_WHENCE_END);
    switch (result) {
        case TPR_SUCCESS:
            break;
        case TPR_PANIC:
            return TPR_PANIC;
        default:
            mLogger.error() << "Failed to call seek at config file [" << result << "]";
            return TPR_SUCCESS;
    }

    auto posExp = mrFileReg.tell(config);
    if (!posExp.has_value()) {
        mLogger.error() << "Failed to call tell at config file [" << posExp.error() << "]";
        return TPR_SUCCESS;
    }
    uint32_t size = posExp.value();

    mJsonData.emplace();
    mJsonData->data = sj::padded_string(size);
    result = mrFileReg.readAt(config, 0, size, reinterpret_cast<std::byte*>(mJsonData->data.data()));
    switch (result) {
        case TPR_SUCCESS:
            break;
        case TPR_PANIC:
            return TPR_PANIC;
        default:
            mLogger.error() << "Failed to call read at config file [" << result << "]";
            return TPR_SUCCESS;
    }

    auto docExp = mJsonData->parser.parse(mJsonData->data);
    if (!docExp.has_value()) {
        mLogger.error() << "Failed to parse config file [" << result << "]";
        mJsonData.reset();
    } else {
        mJsonData->root = docExp.value();
    }

    mLogger.debug() << "Loading config file \"" << mConfPath.string() << "\"";

    return TPR_SUCCESS;
}

Settings::~Settings() {
    std::lock_guard<std::mutex> lock(mMutex);
    flush();
}


Setting Settings::parseSetting(sj::dom::element element) {
    assert(mJsonData.has_value());
    Setting setting{};
    if (element.is_bool()) {
        auto dataExp = element.get_bool();
        if (dataExp.has_value()) {
            setting.data = SettingBool{dataExp.value()};
        }
    } else if (element.is_string()) {
        auto dataExp = element.get_string();
        if (dataExp.has_value()) {
            setting.data = SettingString{std::string(dataExp.value())};
        }
    } else if (element.is_int64()) {
        auto dataExp = element.get_int64();
        if (dataExp.has_value()) {
            setting.data = SettingInt{dataExp.value()};
        }
    } else if (element.is_double()) {
        auto dataExp = element.get_double();
        if (dataExp.has_value()) {
            setting.data = SettingDouble{dataExp.value()};
        }
    } else if (element.is_null()) {
        setting.data = SettingNull{};
    } else if (element.is_array()) {
        auto arrayExp = element.get_array();
        SettingArray data{};
        for (const auto& el : arrayExp.value()) {
            auto [settIt, handleIt] = createUnnamedSetting(el);
            handleIt->second.capability = TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT;
            data.elements.push_back(settIt->first);
        }
        setting.data = data;
    } else if (element.is_object()) {
        setting.data = SettingStruct{};
    }
    return setting;
}


std::pair<
    std::unordered_map<uint32_t, Setting>::iterator,
    std::unordered_map<uint32_t, SettingHandle>::iterator
> Settings::createNamedSetting(std::string_view name, sj::dom::element element) {
    Setting setting = parseSetting(element);
    setting.name = name;
    setting.element = element;
    SettingHandle handle{};
    handle.setting = mSettingCounter;
    auto settIt = mSettings.try_emplace(mSettingCounter, setting).first;
    auto handleIt = mSettingHandles.try_emplace(mHandleCounter, handle).first;
    mSettingCounter++;
    mHandleCounter++;
    return std::make_pair(settIt, handleIt);
}


std::pair<
    std::unordered_map<uint32_t, Setting>::iterator,
    std::unordered_map<uint32_t, SettingHandle>::iterator
> Settings::createUnnamedSetting(sj::dom::element element) {
    Setting setting = parseSetting(element);
    setting.element = element;
    SettingHandle handle{};
    handle.setting = mSettingCounter;
    auto settIt = mSettings.try_emplace(mSettingCounter, setting).first;
    auto handleIt = mSettingHandles.try_emplace(mHandleCounter, handle).first;
    mSettingCounter++;
    mHandleCounter++;
    return std::make_pair(settIt, handleIt);
}


expected<TprSetting, TprResult> Settings::createSetting(TprSetting baseSetting, std::string_view name) noexcept {
    if (name.empty()) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_type(baseSetting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);

    std::lock_guard<std::mutex> lock(mMutex);

    TprSetting h;

    try {
        auto handleIt = mSettingHandles.find(get_basic_handle_index(baseSetting));
        if (handleIt == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& handle = handleIt->second;

        if (!(handle.capability & TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT)) return unexpected(TPR_ERROR_NOT_PERMITTED);

        auto baseIt = mSettings.find(handle.setting);
        if (baseIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: setting " << handle.setting
                << " from handle " << handleIt->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        if (!std::holds_alternative<SettingStruct>(baseIt->second.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        SettingStruct& base = std::get<SettingStruct>(baseIt->second.data);

        auto settingIt = base.elements.end();
        try {
            settingIt = std::ranges::find_if(base.elements, [baseIt, name, this](uint32_t value) {
                auto it = mSettings.find(value);
                if (it == mSettings.end()) throw std::runtime_error(std::format(
                    "Corrupted internal structures: Setting[{}] from {} does not appear in mSettings",
                    value, baseIt->second.name
                ));
                return it->second.name == name;
            });
        } catch (const std::exception& e) {
            mLogger.panic() << "Exception: " << e.what() << "\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        Setting* psett = nullptr;

        if (settingIt != base.elements.end()) {
            auto it = mSettings.find(*settingIt);
            if (it == mSettings.end()) {
                mLogger.panic() << "Corrupted internal structures: search result " << *settingIt << " does not appear in mSettings\n";
                mrRunResult.store(TPR_PANIC);
                return unexpected(TPR_PANIC);
            }
            auto& setting = it->second;
            psett = &setting;
            setting.refCount++;
            SettingHandle handle{};
            handle.setting = *settingIt;
            handle.capability = TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT;
            mSettingHandles.try_emplace(mHandleCounter, handle);
            h = construct_basic_handle<TprSetting>(mHandleCounter, 0, handle_type::setting);
            mHandleCounter++;

        } else {
            sj::simdjson_result<sj::dom::element> el;
            if (
                mJsonData.has_value() && baseIt->second.element.has_value() &&
                (el = baseIt->second.element->at_key(name)).has_value()
            ) {
                auto [settIt, handleIt] = createNamedSetting(name, el.value());
                psett = &settIt->second;
                handleIt->second.capability = TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT;
                h = construct_basic_handle<TprSetting>(handleIt->first, 0, handle_type::setting);
            } else {
                Setting setting{};
                setting.name = name;
                SettingHandle handle{};
                handle.setting = mSettingCounter;
                handle.capability = TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT;
                psett = &mSettings.try_emplace(mSettingCounter, setting).first->second;
                mSettingHandles.try_emplace(mHandleCounter, handle);
                h = construct_basic_handle<TprSetting>(mHandleCounter, 0, handle_type::setting);
                mSettingCounter++;
                mHandleCounter++;
            }

            base.elements.push_back(get_basic_handle_index(h));

            if (mJsonData.has_value()) {
                auto l = mLogger.trace();
                l << "Created setting " << name << " = ";
                printSettingValue(l, *psett);
                l << "\n";
            } else {
                mLogger.trace() << "Created new setting " << name << "\n";
            }
        }

        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


expected<TprSetting, TprResult> Settings::readSetting(TprSetting baseSetting, std::string_view name) noexcept {
    if (name.empty()) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_type(baseSetting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);

    std::lock_guard<std::mutex> lock(mMutex);

    TprSetting h;

    try {
        auto handleIt = mSettingHandles.find(get_basic_handle_index(baseSetting));
        if (handleIt == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& handle = handleIt->second;

        auto baseIt = mSettings.find(handle.setting);
        if (baseIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: setting " << handle.setting
                << " from handle " << handleIt->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        if (!std::holds_alternative<SettingStruct>(baseIt->second.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        SettingStruct& base = std::get<SettingStruct>(baseIt->second.data);

        auto settingIt = base.elements.end();
        try {
            settingIt = std::ranges::find_if(base.elements, [baseIt, name, this](uint32_t value) {
                auto it = mSettings.find(value);
                if (it == mSettings.end()) throw std::runtime_error(std::format(
                    "Corrupted internal structures: Setting[{}] from {} does not appear in mSettings",
                    value, baseIt->second.name
                ));
                return it->second.name == name;
            });
        } catch (const std::exception& e) {
            mLogger.panic() << "Exception: " << e.what() << "\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        Setting* psett = nullptr;

        if (settingIt != base.elements.end()) {
            auto it = mSettings.find(*settingIt);
            if (it == mSettings.end()) {
                mLogger.panic() << "Corrupted internal structures: search result " << *settingIt << " does not appear in mSettings\n";
                mrRunResult.store(TPR_PANIC);
                return unexpected(TPR_PANIC);
            }
            auto& setting = it->second;
            psett = &setting;
            setting.refCount++;
            SettingHandle handle{};
            handle.setting = *settingIt;
            mSettingHandles.try_emplace(mHandleCounter, handle);
            h = construct_basic_handle<TprSetting>(mHandleCounter, 0, handle_type::setting);
            mHandleCounter++;

        } else {
            sj::simdjson_result<sj::dom::element> el;
            if (
                mJsonData.has_value() && baseIt->second.element.has_value() &&
                (el = baseIt->second.element->at_key(name)).has_value()
            ) {
                auto [settIt, handleIt] = createNamedSetting(name, el.value());
                psett = &settIt->second;
                h = construct_basic_handle<TprSetting>(handleIt->first, 0, handle_type::setting);
            } else {
                Setting setting{};
                setting.name = name;
                SettingHandle handle{};
                handle.setting = mSettingCounter;
                psett = &mSettings.try_emplace(mSettingCounter, setting).first->second;
                mSettingHandles.try_emplace(mHandleCounter, handle);
                h = construct_basic_handle<TprSetting>(mHandleCounter, 0, handle_type::setting);
                mSettingCounter++;
                mHandleCounter++;
            }

            base.elements.push_back(get_basic_handle_index(h));

            if (mJsonData.has_value()) {
                auto l = mLogger.trace();
                l << "Read setting " << name << " = ";
                printSettingValue(l, *psett);
                l << "\n";
            } else {
                mLogger.trace() << "Read new setting " << name << "\n";
            }
        }

        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


TprSetting Settings::getRoot() const {
    return mRootSetting;
}


void Settings::printSettingValue(LogEntry& log, const Setting& setting) {
    std::visit(overload{
        [&log](const SettingDouble& value) { log << value.value; },
        [&log](const SettingInt& value) { log << value.value; },
        [&log](const SettingNull& value) { log << "null"; },
        [&log](const SettingUnset& value) { log << "unset"; },
        [&log](const SettingBool& value) { log << (value.value ? "true" : "false"); },
        [&log](const SettingString& value) { log << "\"" << value.value << "\""; },
        [&log](const SettingStruct& value) { log << "struct"; },
        [&log, this, &setting](const SettingArray& value) {
            log << "array [";
            for (auto it = value.elements.begin(); it != value.elements.end(); it++) {
                if (it != value.elements.begin()) log << ", ";
                auto f = mSettings.find(*it);
                if (f == mSettings.end()) throw std::runtime_error(std::format(
                    "Corrupted internal structures: Setting[{}] from {} does not appear in mSettings",
                    *it, setting.name
                ));
                printSettingValue(log, f->second);
            }
            log << "]";
        }
    }, setting.data);
}


void Settings::destroySetting(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        destroySettingById(get_basic_handle_index(setting));
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
    }
}


void Settings::destroySettingById(uint32_t id) {
    if (id > mHandleCounter) return;
    auto it = mSettingHandles.find(id);
    if (it == mSettingHandles.end()) return;
    auto settIt = mSettings.find(it->second.setting);
    if (settIt == mSettings.end()) return;  // must not happen
    auto& setting = settIt->second;
    setting.refCount--;
    if (setting.refCount == 0) {
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        mSettings.erase(settIt);
    }
    mSettingHandles.erase(it);
}


expected<TprSettingType, TprResult> Settings::getSettingType(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        return std::visit(overload{
            [](const SettingDouble& value) { return TPR_SETTING_TYPE_DOUBLE; },
            [](const SettingInt& value) { return TPR_SETTING_TYPE_INTEGER; },
            [](const SettingUnset& value) { return TPR_SETTING_TYPE_UNSET; },
            [](const SettingNull& value) { return TPR_SETTING_TYPE_NULL; },
            [](const SettingBool& value) { return TPR_SETTING_TYPE_BOOL; },
            [](const SettingString& value) { return TPR_SETTING_TYPE_STRING; },
            [](const SettingStruct& value) { return TPR_SETTING_TYPE_STRUCT; },
            [](const SettingArray& value) { return TPR_SETTING_TYPE_ARRAY; }
        }, setting.data);
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<double, TprResult> Settings::getSettingDouble(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingDouble>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingDouble>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<int64_t, TprResult> Settings::getSettingInteger(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingInt>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingInt>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprBool8, TprResult> Settings::getSettingBool(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingBool>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingBool>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<uint32_t, TprResult> Settings::getSettingStringSize(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingString>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingString>(setting.data).value.size();
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

TprResult Settings::copySettingString(TprSetting setting, char* pData) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingString>(setting.data)) return TPR_ERROR_WRONG_TYPE;
        auto& s = std::get<SettingString>(setting.data);
        std::strncpy(pData, s.value.data(), s.value.size());
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}


TprResult Settings::setSettingDouble(TprSetting setting, double data) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingDouble{data};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingInteger(TprSetting setting, int64_t data) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingInt{data};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

expected<std::string, TprResult> Settings::getSettingString(TprSetting setting) {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingString>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingString>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


TprResult Settings::setSettingBool(TprSetting setting, TprBool8 data) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingBool{(data == 0) ? false : true};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingString(TprSetting setting, const char* pData) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingString(pData);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingNull(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingNull{};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Settings::unsetSetting(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingUnset{};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Settings::setSettingStruct(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingStruct{};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}


TprResult Settings::setSettingArray(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        std::visit(overload{
            [this](SettingArray& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](SettingStruct& data) {
                for (auto el : data.elements) destroySettingById(el);
            },
            [this](auto& data) {}
        }, setting.data);
        setting.data = SettingArray{};
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}


double Settings::getSettingDoubleOr(TprSetting setting, double fallback) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return fallback;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return fallback;
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingDouble>(setting.data)) return fallback;
        return std::get<SettingDouble>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        return fallback;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        return fallback;
    }
}

int64_t Settings::getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return fallback;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return fallback;
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingInt>(setting.data)) return fallback;
        return std::get<SettingInt>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        return fallback;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        return fallback;
    }
}

TprBool8 Settings::getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return fallback;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return fallback;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return fallback;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return fallback;
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingBool>(setting.data)) return fallback;
        return std::get<SettingBool>(setting.data).value;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        return fallback;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        return fallback;
    }
}


void Settings::writeSetting(std::ostringstream& stream, size_t identation, const Setting& setting) {
    std::visit(overload{
        [&stream](const SettingDouble& value) { stream << value.value; },
        [&stream](const SettingInt& value) { stream << value.value; },
        [&stream](const SettingNull& value) { stream << "null"; },
        [&stream](const SettingUnset& value) { /* unset */ },
        [&stream](const SettingBool& value)  { stream << (value.value ? "true" : "false"); },
        [&stream](const SettingString& value)  { stream << "\"" << value.value << "\""; },
        [&setting, &stream, identation, this](const SettingStruct& value)  {
            if (value.elements.empty()) {
                stream << "{}";
            } else {
                stream << "{\n";
                bool first = true;
                for (auto it = value.elements.begin(); it != value.elements.end(); it++) {
                    auto f = mSettings.find(*it);
                    if (f == mSettings.end()) continue;  // mustn't happen
                    if (!std::holds_alternative<SettingUnset>(f->second.data)) {
                        if (!first) {
                            stream << ",\n";
                        }
                        first = false;
                        stream << std::string((identation + 1) * 4, ' ');
                        stream << "\"" << f->second.name << "\": ";
                        writeSetting(stream, identation + 1, f->second);
                    }
                }
                stream << "\n" << std::string(identation * 4, ' ') << "}";
            }
        },
        [&setting, &stream, identation, this](const SettingArray& value) {
            if (value.elements.empty()) {
                stream << "[]";
            } else {
                stream << "[\n";
                bool first = true;
                for (auto it = value.elements.begin(); it != value.elements.end(); it++) {
                    auto f = mSettings.find(*it);
                    if (f == mSettings.end()) continue;  // mustn't happen
                    if (!std::holds_alternative<SettingUnset>(f->second.data)) {
                        if (!first) {
                            stream << ",\n";
                        }
                        first = false;
                        stream << std::string((identation + 1) * 4, ' ');
                        writeSetting(stream, identation + 1, f->second);
                    }
                }
                stream << "\n" << std::string(identation * 4, ' ') << "]";
            }
        }
    }, setting.data);
}


TprResult Settings::flush() {
    if (!mFlushConfig) return TPR_SUCCESS;

    TprResult result;
    mLogger.debug() << "Writing config file to \"" << mConfPath.string() << "\"\n";

    auto openExp = mrFileReg.openFile(mConfPath, TPR_OPEN_FILE_SYNC_FLAG_BIT | TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT);
    if (!openExp.has_value()) {
        mLogger.error() << "Failed to open config file [" << openExp.error() << "]\n";
        return TPR_SUCCESS;
    }
    TprFile config = openExp.value();
    scope_exit closeConfig([&]() { mrFileReg.closeFile(config); });

    std::ostringstream data;
    auto rootIt = mSettings.find(get_basic_handle_index(mRootSetting));
    if (rootIt == mSettings.end()) {
        mLogger.panic() << "Corrupted internal structures: mRootSetting doesn't appear in mSettings";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    writeSetting(data, 0, rootIt->second);
    std::string string = data.str();

    result = mrFileReg.resize(config, string.size());
    switch (result) {
        case TPR_SUCCESS:
            break;
        case TPR_PANIC:
            return TPR_PANIC;
        default:
            mLogger.error() << "Failed to call resize at config file [" << result << "]";
            return TPR_SUCCESS;
    }

    result = mrFileReg.write(config, string.size(), reinterpret_cast<const std::byte*>(string.c_str()));
    switch (result) {
        case TPR_SUCCESS:
            break;
        case TPR_PANIC:
            return TPR_PANIC;
        default:
            mLogger.error() << "Failed to call write at config file [" << result << "]";
            return TPR_SUCCESS;
    }

    return TPR_SUCCESS;
}


void Settings::finalizeRead() {
    mLogger.debug() << "Finalizing read from config file\n";
    std::lock_guard<std::mutex> lock(mMutex);
    mJsonData.reset();
    for (auto& [id, setting] : mSettings) {
        setting.element.reset();
    }
}

double Settings::createSettingDoubleOr(TprSetting baseSetting, std::string_view name, double fallback) noexcept {
    auto createExp = createSetting(baseSetting, name);
    if (!createExp.has_value()) return fallback;
    return getSettingDoubleOr(createExp.value(), fallback);
}


int64_t Settings::createSettingIntegerOr(TprSetting baseSetting, std::string_view name, int64_t fallback) noexcept {
    auto createExp = createSetting(baseSetting, name);
    if (!createExp.has_value()) return fallback;
    return getSettingIntegerOr(createExp.value(), fallback);
}


TprBool8 Settings::createSettingBoolOr(TprSetting baseSetting, std::string_view name, TprBool8 fallback) noexcept {
    auto createExp = createSetting(baseSetting, name);
    if (!createExp.has_value()) return fallback;
    return getSettingBoolOr(createExp.value(), fallback);
}


std::string Settings::createSettingStringOr(TprSetting baseSetting, std::string_view name, std::string fallback) noexcept {
    auto createExp = createSetting(baseSetting, name);
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


expected<uint32_t, TprResult> Settings::getSettingArraySize(TprSetting setting) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingArray>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        return std::get<SettingArray>(setting.data).elements.size();
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


expected<TprSetting, TprResult> Settings::getSettingArrayElement(TprSetting setting, uint32_t index) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingArray>(setting.data)) return unexpected(TPR_ERROR_WRONG_TYPE);
        auto& array = std::get<SettingArray>(setting.data);
        if (array.elements.size() <= index) return unexpected(TPR_ERROR_OUT_OF_RANGE);
        return construct_basic_handle<TprSetting>(array.elements[index], 0, handle_type::setting);
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


TprResult Settings::resizeSettingArray(TprSetting setting, uint32_t size) noexcept {
    if (get_basic_handle_type(setting) != handle_type::setting) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_index(setting) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mSettingHandles.find(get_basic_handle_index(setting));
        if (it == mSettingHandles.end()) return TPR_ERROR_INVALID_VALUE;
        if (!(it->second.capability & TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT)) return TPR_ERROR_NOT_PERMITTED;
        auto settIt = mSettings.find(it->second.setting);
        if (settIt == mSettings.end()) {
            mLogger.panic() << "Corrupted internal structures: Setting " << it->second.setting
                << "from handle " << it->first << " does not appear in mSettings\n";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        auto& setting = settIt->second;
        if (!std::holds_alternative<SettingArray>(setting.data)) return TPR_ERROR_WRONG_TYPE;
        auto& array = std::get<SettingArray>(setting.data);
        if (array.elements.size() > size) {
            for (uint32_t i = size; i < array.elements.size(); i++) {
                destroySettingById(array.elements[i]);
            }
            array.elements.resize(size);
        } else if (array.elements.size() < size) {
            uint32_t oldSize = array.elements.size();
            array.elements.resize(size);
            for (uint32_t i = oldSize; i < array.elements.size(); i++) {
                Setting setting{};
                SettingHandle handle{};
                handle.setting = mSettingCounter;
                mSettings.try_emplace(mSettingCounter, setting);
                mSettingHandles.try_emplace(mHandleCounter, handle);
                array.elements[i] = mSettingCounter;
                mSettingCounter++;
                mHandleCounter++;
            }
        }
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what() << "\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception\n";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}


