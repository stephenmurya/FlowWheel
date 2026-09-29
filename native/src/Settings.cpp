#include "Settings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string_view>
#include <vector>

namespace flowwheel::native {
namespace {

constexpr wchar_t kSection[] = L"FlowWheel";
constexpr wchar_t kExcludedAppsSection[] = L"ExcludedApps";
constexpr wchar_t kGameExceptionsSection[] = L"GameBypassExceptions";

bool ReadBool(const wchar_t* key, bool fallback, const std::wstring& filePath) noexcept {
    return GetPrivateProfileIntW(kSection, key, fallback ? 1 : 0, filePath.c_str()) != 0;
}

int ReadInt(const wchar_t* key, int fallback, const std::wstring& filePath) noexcept {
    return GetPrivateProfileIntW(kSection, key, fallback, filePath.c_str());
}

double ReadDouble(const wchar_t* key, double fallback, const std::wstring& filePath) noexcept {
    wchar_t buffer[64]{};
    const auto fallbackText = std::to_wstring(fallback);
    GetPrivateProfileStringW(kSection, key, fallbackText.c_str(), buffer,
                             static_cast<DWORD>(std::size(buffer)), filePath.c_str());
    wchar_t* end = nullptr;
    const double value = std::wcstod(buffer, &end);
    return end != buffer && std::isfinite(value) ? value : fallback;
}

std::vector<std::wstring> ReadList(const wchar_t* section, const std::wstring& filePath) {
    // Profile APIs return a double-NUL terminated list for a null key.
    std::vector<wchar_t> buffer(4096);
    DWORD count = GetPrivateProfileStringW(section, nullptr, nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()), filePath.c_str());
    if (count == 0) {
        return {};
    }
    if (count >= buffer.size() - 2) {
        buffer.resize(16384);
        count = GetPrivateProfileStringW(section, nullptr, nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()), filePath.c_str());
    }

    std::vector<std::wstring> values;
    for (std::size_t offset = 0; offset < count;) {
        std::wstring value(buffer.data() + offset);
        const auto valueLength = value.size();
        if (!value.empty()) {
            values.push_back(std::move(value));
        }
        offset += valueLength + 1;
    }
    return values;
}

void WriteBool(const wchar_t* key, bool value, const std::wstring& filePath) noexcept {
    WritePrivateProfileStringW(kSection, key, value ? L"1" : L"0", filePath.c_str());
}

void WriteDouble(const wchar_t* key, double value, const std::wstring& filePath) noexcept {
    const auto text = std::to_wstring(value);
    WritePrivateProfileStringW(kSection, key, text.c_str(), filePath.c_str());
}

void WriteList(const wchar_t* section, const std::vector<std::wstring>& values,
               const std::wstring& filePath) noexcept {
    WritePrivateProfileStringW(section, nullptr, nullptr, filePath.c_str());
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto key = std::to_wstring(index);
        WritePrivateProfileStringW(section, key.c_str(), values[index].c_str(), filePath.c_str());
    }
}

double ClampFinite(double value, double fallback, double minimum, double maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool SyncStartupRegistration(const std::wstring& valueName, bool enabled) noexcept {
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY key = nullptr;
    if (!enabled) {
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
            // No Run key means there is nothing to remove.
            return true;
        }
        const LONG result = RegDeleteValueW(key, valueName.c_str());
        RegCloseKey(key);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        RegCloseKey(key);
        return false;
    }
    const std::wstring command = L"\"" + std::wstring(path.data(), length) + L"\"";
    const LONG result = RegSetValueExW(key, valueName.c_str(), 0, REG_SZ,
                                       reinterpret_cast<const BYTE*>(command.c_str()),
                                       static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

} // namespace

SettingsStore::SettingsStore(std::wstring applicationName)
    : applicationName_(std::move(applicationName)),
      filePath_(SettingsDirectory(applicationName_) + L"\\settings.ini") {}

const std::wstring& SettingsStore::FilePath() const noexcept {
    return filePath_;
}

Settings SettingsStore::Defaults() {
    return Settings{};
}

Settings SettingsStore::Load() const noexcept {
    Settings settings = Defaults();
    try {
        settings.enabled = ReadBool(L"Enabled", settings.enabled, filePath_);
        settings.stepSize = ClampFinite(ReadDouble(L"StepSize", settings.stepSize, filePath_),
                                        settings.stepSize, 1.0, 1200.0);
        settings.animationTimeMs = static_cast<std::uint32_t>(std::clamp(
            ReadInt(L"AnimationTimeMs", static_cast<int>(settings.animationTimeMs), filePath_),
            20, 2000));
        settings.pulseAlgorithm = ReadBool(L"PulseAlgorithm", settings.pulseAlgorithm, filePath_);
        settings.pulseScale = ClampFinite(ReadDouble(L"PulseScale", settings.pulseScale, filePath_),
                                          settings.pulseScale, 0.0, 12.0);
        settings.pulseNormalize = ClampFinite(
            ReadDouble(L"PulseNormalize", settings.pulseNormalize, filePath_), settings.pulseNormalize,
            0.0, 12.0);
        settings.accelerationEnabled = ReadBool(L"AccelerationEnabled", settings.accelerationEnabled, filePath_);
        settings.accelerationDelta = ClampFinite(
            ReadDouble(L"AccelerationDelta", settings.accelerationDelta, filePath_), settings.accelerationDelta,
            0.0, 2400.0);
        settings.accelerationMax = ClampFinite(
            ReadDouble(L"AccelerationMax", settings.accelerationMax, filePath_), settings.accelerationMax,
            1.0, 8.0);
        settings.reverseDirection = ReadBool(L"ReverseDirection", settings.reverseDirection, filePath_);
        settings.horizontalShiftKey = ReadBool(L"HorizontalShiftKey", settings.horizontalShiftKey, filePath_);
        settings.horizontalSmoothing = ReadBool(L"HorizontalSmoothing", settings.horizontalSmoothing, filePath_);
        settings.autoDisableGames = ReadBool(L"AutoDisableGames", settings.autoDisableGames, filePath_);
        settings.launchOnLogin = ReadBool(L"LaunchOnLogin", settings.launchOnLogin, filePath_);
        settings.showNotifications = ReadBool(L"ShowNotifications", settings.showNotifications, filePath_);
        settings.excludedAppPaths = ReadList(kExcludedAppsSection, filePath_);
        settings.gameBypassExceptions = ReadList(kGameExceptionsSection, filePath_);
    } catch (...) {
        return Defaults();
    }
    return settings;
}

bool SettingsStore::Save(const Settings& input) const noexcept {
    try {
        Settings settings = input;
        settings.stepSize = ClampFinite(settings.stepSize, 120.0, 1.0, 1200.0);
        settings.animationTimeMs = std::clamp<std::uint32_t>(settings.animationTimeMs, 20, 2000);
        settings.pulseScale = ClampFinite(settings.pulseScale, 3.0, 0.0, 12.0);
        settings.pulseNormalize = ClampFinite(settings.pulseNormalize, 1.0, 0.0, 12.0);
        settings.accelerationDelta = ClampFinite(settings.accelerationDelta, 75.0, 0.0, 2400.0);
        settings.accelerationMax = ClampFinite(settings.accelerationMax, 2.0, 1.0, 8.0);

        if (!EnsureDirectory(SettingsDirectory(applicationName_))) {
            return false;
        }
        WriteBool(L"Enabled", settings.enabled, filePath_);
        WriteDouble(L"StepSize", settings.stepSize, filePath_);
        WritePrivateProfileStringW(kSection, L"AnimationTimeMs",
                                   std::to_wstring(settings.animationTimeMs).c_str(), filePath_.c_str());
        WriteBool(L"PulseAlgorithm", settings.pulseAlgorithm, filePath_);
        WriteDouble(L"PulseScale", settings.pulseScale, filePath_);
        WriteDouble(L"PulseNormalize", settings.pulseNormalize, filePath_);
        WriteBool(L"AccelerationEnabled", settings.accelerationEnabled, filePath_);
        WriteDouble(L"AccelerationDelta", settings.accelerationDelta, filePath_);
        WriteDouble(L"AccelerationMax", settings.accelerationMax, filePath_);
        WriteBool(L"ReverseDirection", settings.reverseDirection, filePath_);
        WriteBool(L"HorizontalShiftKey", settings.horizontalShiftKey, filePath_);
        WriteBool(L"HorizontalSmoothing", settings.horizontalSmoothing, filePath_);
        WriteBool(L"AutoDisableGames", settings.autoDisableGames, filePath_);
        WriteBool(L"LaunchOnLogin", settings.launchOnLogin, filePath_);
        WriteBool(L"ShowNotifications", settings.showNotifications, filePath_);
        WriteList(kExcludedAppsSection, settings.excludedAppPaths, filePath_);
        WriteList(kGameExceptionsSection, settings.gameBypassExceptions, filePath_);
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, filePath_.c_str());
        // Keep the registry mirror in sync even when changes originate in the
        // settings window rather than the tray menu.
        return SyncStartupRegistration(applicationName_, settings.launchOnLogin);
    } catch (...) {
        return false;
    }
}

bool SettingsStore::Reset() const noexcept {
    return Save(Defaults());
}

std::wstring SettingsStore::SettingsDirectory(const std::wstring& applicationName) {
    PWSTR appData = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &appData))) {
        result.assign(appData);
        CoTaskMemFree(appData);
    }
    if (result.empty()) {
        wchar_t fallback[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", fallback, MAX_PATH);
        if (length != 0 && length < MAX_PATH) {
            result.assign(fallback, length);
        }
    }
    if (result.empty()) {
        result = L".";
    }
    return result + L"\\" + applicationName;
}

bool SettingsStore::EnsureDirectory(const std::wstring& path) noexcept {
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return !error;
}

} // namespace flowwheel::native
