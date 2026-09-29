#include "SettingsWindow.h"

#include <commctrl.h>
#include <algorithm>
#include <string>

namespace flowwheel {
namespace {
constexpr wchar_t kClass[] = L"FlowWheel.Native.SettingsWindow";
constexpr int kPreset = 100;
constexpr int kSensitivity = 101;
constexpr int kDuration = 102;
constexpr int kAcceleration = 103;
constexpr int kEasing = 104;
constexpr int kSmoothHorizontal = 105;
constexpr int kShiftHorizontal = 106;
constexpr int kReverse = 107;
constexpr int kGames = 108;
constexpr int kStartup = 109;
constexpr int kNotifications = 110;
constexpr int kReset = 111;
constexpr int kClose = 112;
constexpr int kStatus = 113;

HWND CreateChild(const wchar_t* klass, const wchar_t* text, DWORD style, DWORD ex,
                 HWND parent, HINSTANCE instance, int id, int x, int y, int w, int h) {
    return CreateWindowExW(ex, klass, text, style, x, y, w, h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}
} // namespace

SettingsWindow::SettingsWindow(HINSTANCE instance, HWND owner, ISettings& settings,
                               IScrollEngine& engine, std::function<void()> onChanged)
    : instance_(instance), owner_(owner), settings_(settings), engine_(engine), onChanged_(std::move(onChanged)) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(248, 249, 251));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
}

SettingsWindow::~SettingsWindow() {
    if (window_) DestroyWindow(window_);
}

void SettingsWindow::Show() {
    if (!window_) {
        window_ = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, L"FlowWheel settings",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 520, 560, owner_, nullptr,
                                  instance_, this);
        if (!window_) return;
        BuildControls();
        LoadControls();
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
}

void SettingsWindow::Close() {
    if (window_) ShowWindow(window_, SW_HIDE);
}

HWND SettingsWindow::AddLabel(const wchar_t* text, int x, int y, int width, int height) {
    return CreateChild(L"STATIC", text, WS_CHILD | WS_VISIBLE, 0, window_, instance_, 0, x, y, width, height);
}

HWND SettingsWindow::AddCheck(const wchar_t* text, int x, int y, int width, int height, int id) {
    return CreateChild(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                       0, window_, instance_, id, x, y, width, height);
}

HWND SettingsWindow::AddTrack(const wchar_t* text, int x, int y, int width, int min, int max, int value, int id) {
    AddLabel(text, x, y, width, 20);
    HWND track = CreateChild(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_HORZ,
                             0, window_, instance_, id, x, y + 19, width, 28);
    SendMessageW(track, TBM_SETRANGE, TRUE, MAKELONG(min, max));
    SendMessageW(track, TBM_SETPOS, TRUE, value);
    return track;
}

void SettingsWindow::BuildControls() {
    AddLabel(L"Smooth scrolling", 28, 22, 360, 26);
    AddLabel(L"Natural-feeling motion for browsers, documents, and desktop views.", 28, 48, 430, 20);
    AddLabel(L"Profile", 28, 86, 170, 20);
    preset_ = CreateChild(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                          0, window_, instance_, kPreset, 28, 108, 180, 30);
    SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Natural"));
    SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gentle"));
    SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fast"));

    sensitivity_ = AddTrack(L"Sensitivity", 28, 154, 205, 10, 100, 50, kSensitivity);
    duration_ = AddTrack(L"Settling duration (ms)", 28, 216, 205, 80, 800, 260, kDuration);
    acceleration_ = AddTrack(L"Acceleration", 28, 278, 205, 0, 100, 0, kAcceleration);
    AddLabel(L"Easing", 28, 348, 170, 20);
    easing_ = CreateChild(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                          0, window_, instance_, kEasing, 28, 370, 205, 30);
    SendMessageW(easing_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cubic"));
    SendMessageW(easing_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Smooth"));
    SendMessageW(easing_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Linear"));

    smoothHorizontal_ = AddCheck(L"Smooth horizontal scrolling", 270, 110, 220, 24, kSmoothHorizontal);
    shiftHorizontal_ = AddCheck(L"Shift + wheel scrolls horizontally", 270, 144, 220, 24, kShiftHorizontal);
    reverseDirection_ = AddCheck(L"Reverse scrolling direction", 270, 178, 220, 24, kReverse);
    autoGames_ = AddCheck(L"Pause automatically in games", 270, 212, 220, 24, kGames);
    AddLabel(L"Games use raw input and should not receive animation pulses.", 270, 238, 220, 38);
    startup_ = AddCheck(L"Launch FlowWheel when I sign in", 270, 300, 220, 24, kStartup);
    notifications_ = AddCheck(L"Show tray notifications", 270, 334, 220, 24, kNotifications);
    status_ = CreateChild(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, window_, instance_, kStatus, 28, 430, 440, 24);
    CreateChild(L"BUTTON", L"Reset defaults", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, window_, instance_, kReset, 270, 398, 110, 32);
    CreateChild(L"BUTTON", L"Done", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, window_, instance_, kClose, 390, 398, 100, 32);
}

void SettingsWindow::LoadControls() {
    const auto s = settings_.Snapshot();
    SendMessageW(sensitivity_, TBM_SETPOS, TRUE, std::clamp(s.sensitivity, 10, 100));
    SendMessageW(duration_, TBM_SETPOS, TRUE, std::clamp(s.durationMs, 80, 800));
    SendMessageW(acceleration_, TBM_SETPOS, TRUE, std::clamp(s.acceleration, 0, 100));
    SendMessageW(easing_, CB_SETCURSEL, s.easing, 0);
    SendMessageW(preset_, CB_SETCURSEL, s.sensitivity >= 65 ? 2 : s.sensitivity <= 35 ? 1 : 0, 0);
    SendMessageW(smoothHorizontal_, BM_SETCHECK, s.smoothHorizontal ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(shiftHorizontal_, BM_SETCHECK, s.shiftHorizontal ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(reverseDirection_, BM_SETCHECK, s.reverseDirection ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(autoGames_, BM_SETCHECK, s.autoDisableGames ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(startup_, BM_SETCHECK, s.launchOnLogin ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(notifications_, BM_SETCHECK, s.showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);
}

int SettingsWindow::TrackValue(HWND track) { return static_cast<int>(SendMessageW(track, TBM_GETPOS, 0, 0)); }

void SettingsWindow::Commit(bool notify) {
    auto s = settings_.Snapshot();
    s.sensitivity = TrackValue(sensitivity_);
    s.durationMs = TrackValue(duration_);
    s.acceleration = TrackValue(acceleration_);
    s.easing = static_cast<int>(SendMessageW(easing_, CB_GETCURSEL, 0, 0));
    s.smoothHorizontal = SendMessageW(smoothHorizontal_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.shiftHorizontal = SendMessageW(shiftHorizontal_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.reverseDirection = SendMessageW(reverseDirection_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.autoDisableGames = SendMessageW(autoGames_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.launchOnLogin = SendMessageW(startup_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.showNotifications = SendMessageW(notifications_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.Apply(s); settings_.Save(); engine_.ApplySettings(s);
    if (notify && onChanged_) onChanged_();
    SetStatus(L"Saved");
}

void SettingsWindow::ApplyPreset(int preset) {
    settings_.ApplyPreset(preset); settings_.Save();
    const auto s = settings_.Snapshot(); engine_.ApplySettings(s); LoadControls();
    if (onChanged_) onChanged_(); SetStatus(L"Preset applied");
}

void SettingsWindow::ResetDefaults() {
    if (MessageBoxW(window_, L"Restore the Natural defaults? Your app exclusions will be cleared.",
                    L"Reset defaults", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    settings_.ResetDefaults(); settings_.Save();
    const auto s = settings_.Snapshot(); engine_.ApplySettings(s); LoadControls();
    if (onChanged_) onChanged_(); SetStatus(L"Defaults restored");
}

void SettingsWindow::SetStatus(const wchar_t* text) { if (status_) SetWindowTextW(status_, text); }

LRESULT CALLBACK SettingsWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(window, message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kPreset && HIWORD(wParam) == CBN_SELCHANGE) { ApplyPreset(static_cast<int>(SendMessageW(preset_, CB_GETCURSEL, 0, 0))); return 0; }
        if (id == kEasing && HIWORD(wParam) == CBN_SELCHANGE) { Commit(); return 0; }
        if (id >= kSmoothHorizontal && id <= kNotifications && HIWORD(wParam) == BN_CLICKED) { Commit(); return 0; }
        if (id == kReset) { ResetDefaults(); return 0; }
        if (id == kClose) { ShowWindow(window, SW_HIDE); return 0; }
    }
    if (message == WM_HSCROLL && (reinterpret_cast<HWND>(lParam) == sensitivity_ || reinterpret_cast<HWND>(lParam) == duration_ || reinterpret_cast<HWND>(lParam) == acceleration_)) { Commit(); return 0; }
    if (message == WM_CLOSE) { ShowWindow(window, SW_HIDE); return 0; }
    if (message == WM_DESTROY) { window_ = nullptr; return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace flowwheel
