#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace flowwheel::native {

// The settings model deliberately contains only user-facing behaviour. Runtime
// components can copy it without depending on a serialization library.
struct Settings final {
    bool enabled = true;
    double stepSize = 120.0;
    std::uint32_t animationTimeMs = 260;
    bool pulseAlgorithm = true;
    double pulseScale = 3.0;
    double pulseNormalize = 1.0;
    bool accelerationEnabled = false;
    double accelerationDelta = 75.0;
    // Maximum acceleration multiplier (1.0 = no increase, 2.0 = up to 2x).
    // The settings window exposes this as a percentage above the base speed.
    double accelerationMax = 2.0;
    bool reverseDirection = false;
    bool horizontalShiftKey = true;
    bool horizontalSmoothing = true;
    bool autoDisableGames = true;
    bool launchOnLogin = false;
    bool showNotifications = true;

    // Paths are stored as full executable paths. Case-insensitive matching is
    // performed by the policy layer, not by persistence.
    std::vector<std::wstring> excludedAppPaths;
    std::vector<std::wstring> gameBypassExceptions;
};

class SettingsStore final {
public:
    explicit SettingsStore(std::wstring applicationName = L"FlowWheel");

    [[nodiscard]] const std::wstring& FilePath() const noexcept;
    [[nodiscard]] Settings Load() const noexcept;
    [[nodiscard]] bool Save(const Settings& settings) const noexcept;
    [[nodiscard]] bool Reset() const noexcept;

    [[nodiscard]] static Settings Defaults();

private:
    std::wstring applicationName_;
    std::wstring filePath_;

    static std::wstring SettingsDirectory(const std::wstring& applicationName);
    static bool EnsureDirectory(const std::wstring& path) noexcept;
};

} // namespace flowwheel::native
