#include "TrayApp.h"

#include <windows.h>

#include "ScrollEngine.h"
#include "Settings.h"

#include <algorithm>
#include <cmath>

namespace {
using flowwheel::AppRule;
using flowwheel::IScrollEngine;
using flowwheel::ISettings;
using flowwheel::SettingsSnapshot;

class NativeSettingsAdapter final : public ISettings {
public:
    NativeSettingsAdapter() : store_(L"FlowWheel"), value_(store_.Load()) {}

    SettingsSnapshot Snapshot() const override {
        SettingsSnapshot s;
        s.enabled = value_.enabled;
        s.launchOnLogin = value_.launchOnLogin;
        s.showNotifications = value_.showNotifications;
        s.autoDisableGames = value_.autoDisableGames;
        s.smoothHorizontal = value_.horizontalSmoothing;
        s.shiftHorizontal = value_.horizontalShiftKey;
        s.reverseDirection = value_.reverseDirection;
        s.sensitivity = std::clamp(static_cast<int>(value_.stepSize * 50.0 / 120.0), 10, 100);
        s.durationMs = static_cast<int>(value_.animationTimeMs);
        const double multiplier = std::clamp(value_.accelerationMax, 1.0, 8.0);
        s.acceleration = value_.accelerationEnabled
            ? std::clamp(static_cast<int>(std::lround((multiplier - 1.0) * 100.0)), 0, 100)
            : 0;
        s.easing = value_.pulseAlgorithm ? (value_.pulseScale < 2.0 ? 1 : 0) : 2;
        for (const auto& path : value_.excludedAppPaths) s.appRules.push_back({path, false, false});
        for (const auto& path : value_.gameBypassExceptions) s.appRules.push_back({path, true, true});
        return s;
    }

    void Apply(const SettingsSnapshot& s) override {
        value_.enabled = s.enabled;
        value_.launchOnLogin = s.launchOnLogin;
        value_.showNotifications = s.showNotifications;
        value_.autoDisableGames = s.autoDisableGames;
        value_.horizontalSmoothing = s.smoothHorizontal;
        value_.horizontalShiftKey = s.shiftHorizontal;
        value_.reverseDirection = s.reverseDirection;
        value_.stepSize = std::clamp(s.sensitivity * 120.0 / 50.0, 24.0, 240.0);
        value_.animationTimeMs = static_cast<std::uint32_t>(std::clamp(s.durationMs, 20, 2000));
        value_.accelerationEnabled = s.acceleration > 0;
        value_.accelerationMax = 1.0 + std::clamp(static_cast<double>(s.acceleration), 0.0, 100.0) / 100.0;
        value_.pulseAlgorithm = s.easing != 2;
        value_.pulseScale = s.easing == 1 ? 1.5 : 3.0;
        // Snapshot is authoritative. Rebuild the persisted lists so enabling
        // an app removes its old exclusion instead of leaving a stale rule.
        value_.excludedAppPaths.clear();
        value_.gameBypassExceptions.clear();
        for (const auto& rule : s.appRules) {
            if (rule.automaticGameRule && rule.smooth) value_.gameBypassExceptions.push_back(rule.executablePath);
            else if (!rule.smooth) value_.excludedAppPaths.push_back(rule.executablePath);
        }
    }

    void Save() override { (void)store_.Save(value_); }
    void ResetDefaults() override { value_ = flowwheel::native::SettingsStore::Defaults(); }
    void ApplyPreset(int preset) override {
        // Presets intentionally update tuning only; exclusions, game
        // exceptions, startup, and other toggles are user policy.
        switch (preset) {
        case 1:
            value_.stepSize = 100.0;
            value_.animationTimeMs = 340;
            value_.accelerationEnabled = false;
            value_.accelerationMax = 2.0;
            value_.pulseAlgorithm = true;
            value_.pulseScale = 1.5;
            break;
        case 2:
            value_.stepSize = 149.0;
            value_.animationTimeMs = 180;
            value_.accelerationEnabled = true;
            value_.accelerationMax = 1.35;
            value_.pulseAlgorithm = true;
            value_.pulseScale = 3.0;
            break;
        default:
            value_.stepSize = 120.0;
            value_.animationTimeMs = 260;
            value_.accelerationEnabled = false;
            value_.accelerationMax = 2.0;
            value_.pulseAlgorithm = true;
            value_.pulseScale = 3.0;
            break;
        }
    }
    const flowwheel::native::Settings& Value() const { return value_; }

private:
    flowwheel::native::SettingsStore store_;
    flowwheel::native::Settings value_;
};

class NativeEngineAdapter final : public IScrollEngine {
public:
    explicit NativeEngineAdapter(const NativeSettingsAdapter& settings)
        : settings_(settings), engine_(ToOptions(settings.Snapshot())) {}

    bool Start() override { return engine_.Start(); }
    void Stop() override { engine_.Stop(); }
    void SetEnabled(bool enabled) override { auto s = settings_.Snapshot(); s.enabled = enabled; ApplySettings(s); }
    void ApplySettings(const SettingsSnapshot& s) override { engine_.UpdateOptions(ToOptions(s)); }

private:
    static flowwheel::FlowWheelOptions ToOptions(const SettingsSnapshot& s) {
        flowwheel::FlowWheelOptions o;
        o.enabled = s.enabled; o.globalEnabled = s.enabled;
        o.stepSize = s.sensitivity * 120.0 / 50.0; o.animationTime = s.durationMs;
        o.pulseAlgorithm = s.easing != 2; o.pulseScale = s.easing == 1 ? 1.5 : 3.0;
        o.accelerationDefault = s.acceleration > 0;
        o.accelerationMax = 1.0 + std::clamp(s.acceleration, 0, 100) / 100.0;
        o.reverseDirection = s.reverseDirection; o.horizontalSmoothing = s.smoothHorizontal; o.horizontalShiftKey = s.shiftHorizontal;
        o.autoDisableGames = s.autoDisableGames;
        for (const auto& rule : s.appRules) {
            if (rule.automaticGameRule && rule.smooth) o.gameBypassExceptions.insert(rule.executablePath);
            else if (!rule.smooth) o.excludedProcessPaths.insert(rule.executablePath);
        }
        return o;
    }
    const NativeSettingsAdapter& settings_;
    flowwheel::ScrollEngine engine_;
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\FlowWheel");
    if (!mutex) return 2;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    NativeSettingsAdapter settings;
    NativeEngineAdapter engine(settings);
    flowwheel::TrayApp app(settings, engine);
    const int result = app.Run(instance);
    CloseHandle(mutex);
    return result;
}
