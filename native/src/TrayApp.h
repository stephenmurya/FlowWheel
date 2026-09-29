#pragma once

#include <windows.h>
#include <shellapi.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace flowwheel {

// The native shell deliberately talks to a small, UI-facing contract.  The
// settings/engine implementation can remain independent of Win32 controls;
// the native rewrite's Settings and ScrollEngine types can implement this
// contract directly (or through a very small adapter).
struct AppRule {
    std::wstring executablePath;
    bool smooth{false};
    bool automaticGameRule{false};
};

struct SettingsSnapshot {
    bool enabled{true};
    bool launchOnLogin{false};
    bool showNotifications{true};
    bool autoDisableGames{true};
    bool smoothHorizontal{true};
    bool shiftHorizontal{true};
    bool reverseDirection{false};
    int sensitivity{50};
    int durationMs{260};
    int acceleration{0};
    int easing{0}; // 0 cubic, 1 smooth, 2 linear
    std::vector<AppRule> appRules;
};

class ISettings {
public:
    virtual ~ISettings() = default;
    virtual SettingsSnapshot Snapshot() const = 0;
    virtual void Apply(const SettingsSnapshot& snapshot) = 0;
    virtual void Save() = 0;
    virtual void ResetDefaults() = 0;
    virtual void ApplyPreset(int preset) = 0; // 0 Natural, 1 Gentle, 2 Fast
};

class IScrollEngine {
public:
    virtual ~IScrollEngine() = default;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void ApplySettings(const SettingsSnapshot& snapshot) = 0;
};

class SettingsWindow;

class TrayApp {
public:
    TrayApp(ISettings& settings, IScrollEngine& engine);
    ~TrayApp();

    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    // Runs the hidden message window and notification icon. Returns the
    // process exit code. Call only from the process' UI thread.
    int Run(HINSTANCE instance);
    void RequestExit();

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    void CreateTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu(POINT point);
    void RefreshMenuState();
    void RefreshForegroundTarget();
    void ToggleForegroundRule();
    void ShowSettings();
    void SetLaunchOnLogin(bool enabled);
    void Notify(const wchar_t* title, const wchar_t* message);
    std::wstring ForegroundExecutable() const;
    std::wstring ForegroundDisplayName() const;
    bool IsForegroundExcluded(const std::wstring& path) const;

    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    HICON icon_{nullptr};
    UINT taskbarCreatedMessage_{0};
    NOTIFYICONDATAW tray_{};
    HMENU menu_{nullptr};
    UINT foregroundRuleCommand_{0};
    UINT settingsCommand_{0};
    UINT toggleCommand_{0};
    UINT startupCommand_{0};
    UINT exitCommand_{0};
    std::wstring foregroundPath_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    ISettings& settings_;
    IScrollEngine& engine_;
    bool shuttingDown_{false};
};

} // namespace flowwheel
