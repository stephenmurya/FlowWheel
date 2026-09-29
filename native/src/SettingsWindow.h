#pragma once

#include "TrayApp.h"

namespace flowwheel {

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE instance, HWND owner, ISettings& settings,
                   IScrollEngine& engine, std::function<void()> onChanged);
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    void Show();
    void Close();
    bool IsOpen() const { return window_ != nullptr; }

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    void BuildControls();
    void LoadControls();
    void Commit(bool notify = true);
    void ApplyPreset(int preset);
    void ResetDefaults();
    void SetStatus(const wchar_t* text);
    HWND AddLabel(const wchar_t* text, int x, int y, int width, int height);
    HWND AddCheck(const wchar_t* text, int x, int y, int width, int height, int id);
    HWND AddTrack(const wchar_t* text, int x, int y, int width, int min, int max, int value, int id);
    static int TrackValue(HWND track);

    HINSTANCE instance_{nullptr};
    HWND owner_{nullptr};
    HWND window_{nullptr};
    HWND preset_{nullptr};
    HWND sensitivity_{nullptr};
    HWND duration_{nullptr};
    HWND acceleration_{nullptr};
    HWND easing_{nullptr};
    HWND smoothHorizontal_{nullptr};
    HWND shiftHorizontal_{nullptr};
    HWND reverseDirection_{nullptr};
    HWND autoGames_{nullptr};
    HWND startup_{nullptr};
    HWND notifications_{nullptr};
    HWND status_{nullptr};
    ISettings& settings_;
    IScrollEngine& engine_;
    std::function<void()> onChanged_;
};

} // namespace flowwheel
