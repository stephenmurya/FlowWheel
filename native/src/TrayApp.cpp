#include "TrayApp.h"
#include "SettingsWindow.h"
#include "ScrollEngine.h"

#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>
#include <psapi.h>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace flowwheel {
namespace {
constexpr wchar_t kWindowClass[] = L"FlowWheel.Native.TrayWindow";
constexpr UINT kTrayMessage = WM_APP + 0x21;
constexpr UINT kTimerForeground = 0x71;
constexpr UINT kMenuToggle = 1001;
constexpr UINT kMenuForeground = 1002;
constexpr UINT kMenuSettings = 1003;
constexpr UINT kMenuStartup = 1004;
constexpr UINT kMenuExit = 1005;

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return std::towlower(c); });
    return value;
}

std::wstring FileName(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

HICON MakeTrayIcon() {
    // A small in-memory icon keeps the executable independent of an external
    // resource file. The icon is intentionally high contrast in the tray.
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 32, 32);
    HBITMAP old = static_cast<HBITMAP>(SelectObject(dc, color));
    RECT r{0, 0, 32, 32};
    HBRUSH bg = CreateSolidBrush(RGB(35, 39, 48));
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(90, 220, 190));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Arc(dc, 7, 7, 26, 26, 18, 7, 7, 18);
    MoveToEx(dc, 18, 5, nullptr); LineTo(dc, 25, 9); LineTo(dc, 23, 17);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
    SelectObject(dc, old);
    DeleteDC(dc); ReleaseDC(nullptr, screen);

    ICONINFO info{}; info.fIcon = TRUE; info.hbmColor = color;
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, nullptr); info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color); DeleteObject(mask);
    return icon;
}

std::wstring FriendlyDisplayName(const std::wstring& path) {
    SHFILEINFOW info{};
    if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
                       SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES) != 0 && info.szDisplayName[0] != L'\0') {
        return info.szDisplayName;
    }
    return FileName(path);
}
} // namespace

TrayApp::TrayApp(ISettings& settings, IScrollEngine& engine)
    : settings_(settings), engine_(engine) {}

TrayApp::~TrayApp() {
    if (window_) DestroyWindow(window_);
    if (icon_) DestroyIcon(icon_);
    if (menu_) DestroyMenu(menu_);
}

int TrayApp::Run(HINSTANCE instance) {
    instance_ = instance;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    RegisterClassExW(&wc);

    window_ = CreateWindowExW(0, kWindowClass, L"FlowWheel", WS_OVERLAPPED,
                              0, 0, 1, 1, nullptr, nullptr, instance_, this);
    if (!window_) return 3;
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    CreateTrayIcon();
    SetTimer(window_, kTimerForeground, 350, nullptr);
    RefreshForegroundTarget();

    const auto snapshot = settings_.Snapshot();
    engine_.ApplySettings(snapshot);
    if (!engine_.Start()) Notify(L"FlowWheel", L"Could not start the mouse hook; scrolling remains unchanged.");

    MSG message{};
    while (!shuttingDown_ && GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void TrayApp::RequestExit() {
    if (window_) PostMessageW(window_, WM_CLOSE, 0, 0);
}

void TrayApp::CreateTrayIcon() {
    // Explorer can broadcast TaskbarCreated more than once. Tear down the old
    // icon/menu before re-adding so the tray never accumulates duplicates.
    if (tray_.hWnd) Shell_NotifyIconW(NIM_DELETE, &tray_);
    if (icon_) { DestroyIcon(icon_); icon_ = nullptr; }
    if (menu_) { DestroyMenu(menu_); menu_ = nullptr; }
    tray_ = NOTIFYICONDATAW{};
    icon_ = MakeTrayIcon();
    tray_ = NOTIFYICONDATAW{};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = icon_;
    StringCchCopyW(tray_.szTip, ARRAYSIZE(tray_.szTip), L"FlowWheel — smooth scrolling");
    Shell_NotifyIconW(NIM_ADD, &tray_);
    menu_ = CreatePopupMenu();
    AppendMenuW(menu_, MF_STRING, kMenuToggle, L"Smooth scrolling");
    AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu_, MF_STRING, kMenuForeground, L"App scrolling rule");
    AppendMenuW(menu_, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenuW(menu_, MF_STRING, kMenuStartup, L"Launch on login");
    AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu_, MF_STRING, kMenuExit, L"Exit FlowWheel");
    RefreshMenuState();
}

void TrayApp::RemoveTrayIcon() {
    if (tray_.hWnd) Shell_NotifyIconW(NIM_DELETE, &tray_);
}

void TrayApp::ShowTrayMenu(POINT point) {
    SetForegroundWindow(window_);
    RefreshForegroundTarget();
    RefreshMenuState();
    TrackPopupMenu(menu_, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
}

void TrayApp::RefreshMenuState() {
    if (!menu_) return;
    const auto s = settings_.Snapshot();
    CheckMenuItem(menu_, kMenuToggle, MF_BYCOMMAND | (s.enabled ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu_, kMenuStartup, MF_BYCOMMAND | (s.launchOnLogin ? MF_CHECKED : MF_UNCHECKED));
    const bool hasForeground = !foregroundPath_.empty();
    EnableMenuItem(menu_, kMenuForeground, MF_BYCOMMAND | (hasForeground ? MF_ENABLED : MF_GRAYED));
    std::wstring text = L"App scrolling rule unavailable";
    if (hasForeground) {
        text = IsForegroundExcluded(foregroundPath_) ? L"Enable smooth scrolling in " : L"Disable smooth scrolling in ";
        text += ForegroundDisplayName();
    }
    MENUITEMINFOW item{sizeof(item)}; item.fMask = MIIM_STRING;
    item.dwTypeData = text.data();
    SetMenuItemInfoW(menu_, kMenuForeground, FALSE, &item);
}

void TrayApp::RefreshForegroundTarget() {
    if (!window_) return;
    HWND foreground = GetForegroundWindow();
    if (foreground == window_ || IsChild(window_, foreground)) return;
    DWORD pid = 0; GetWindowThreadProcessId(foreground, &pid);
    if (!pid || pid == GetCurrentProcessId()) return;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return;
    wchar_t path[32768]; DWORD length = ARRAYSIZE(path);
    if (QueryFullProcessImageNameW(process, 0, path, &length)) foregroundPath_.assign(path, length);
    CloseHandle(process);
}

bool TrayApp::IsForegroundExcluded(const std::wstring& path) const {
    const auto s = settings_.Snapshot();
    const auto wanted = Lower(path);
    bool gameException = false;
    for (const auto& rule : s.appRules) {
        if (Lower(rule.executablePath) != wanted) continue;
        if (rule.automaticGameRule) gameException = rule.smooth;
        else if (!rule.smooth) return true;
    }
    return s.autoDisableGames && IsLikelyGameProcess(path) && !gameException;
}

std::wstring TrayApp::ForegroundDisplayName() const {
    return FriendlyDisplayName(foregroundPath_);
}

void TrayApp::ToggleForegroundRule() {
    if (foregroundPath_.empty()) return;
    auto snapshot = settings_.Snapshot();
    const auto wanted = Lower(foregroundPath_);
    auto it = std::find_if(snapshot.appRules.begin(), snapshot.appRules.end(), [&](const AppRule& r) {
        return Lower(r.executablePath) == wanted;
    });
    const bool automaticallyDetectedGame = snapshot.autoDisableGames && IsLikelyGameProcess(foregroundPath_);
    if (automaticallyDetectedGame) {
        if (it != snapshot.appRules.end() && it->automaticGameRule) {
            snapshot.appRules.erase(it); // restore the automatic bypass
        } else if (it == snapshot.appRules.end()) {
            // A game is bypassed by default; this persisted exception opts it in.
            snapshot.appRules.push_back({foregroundPath_, true, true});
        } else {
            // Convert an older explicit rule into an opt-in exception rather
            // than leaving the detected game disabled.
            it->smooth = true;
            it->automaticGameRule = true;
        }
    } else if (it == snapshot.appRules.end()) {
        snapshot.appRules.push_back({foregroundPath_, false, false});
    } else {
        it->smooth = !it->smooth;
    }
    settings_.Apply(snapshot); settings_.Save(); engine_.ApplySettings(snapshot);
    RefreshMenuState();
    if (snapshot.showNotifications) Notify(L"FlowWheel", IsForegroundExcluded(foregroundPath_)
        ? L"Smooth scrolling disabled for this app." : L"Smooth scrolling enabled for this app.");
}

void TrayApp::ShowSettings() {
    if (!settingsWindow_) {
        settingsWindow_ = std::make_unique<SettingsWindow>(instance_, window_, settings_, engine_, [this] {
            RefreshMenuState();
        });
    }
    settingsWindow_->Show();
}

void TrayApp::SetLaunchOnLogin(bool enabled) {
    auto snapshot = settings_.Snapshot(); snapshot.launchOnLogin = enabled;
    settings_.Apply(snapshot); settings_.Save();
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        if (enabled) {
            wchar_t path[MAX_PATH]; GetModuleFileNameW(instance_, path, ARRAYSIZE(path));
            std::wstring value = L"\"" + std::wstring(path) + L"\"";
            RegSetValueExW(key, L"FlowWheel", 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        } else RegDeleteValueW(key, L"FlowWheel");
        RegCloseKey(key);
    }
    RefreshMenuState();
}

void TrayApp::Notify(const wchar_t* title, const wchar_t* message) {
    auto snapshot = settings_.Snapshot();
    if (!snapshot.showNotifications) return;
    tray_.uFlags = NIF_INFO;
    StringCchCopyW(tray_.szInfoTitle, ARRAYSIZE(tray_.szInfoTitle), title);
    StringCchCopyW(tray_.szInfo, ARRAYSIZE(tray_.szInfo), message);
    tray_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

LRESULT CALLBACK TrayApp::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    TrayApp* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<TrayApp*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->HandleMessage(window, message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT TrayApp::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_) { CreateTrayIcon(); return 0; }
    if (message == kTrayMessage) {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) { POINT p{}; GetCursorPos(&p); ShowTrayMenu(p); }
        else if (lParam == WM_LBUTTONDBLCLK) ShowSettings();
        return 0;
    }
    if (message == WM_TIMER && wParam == kTimerForeground) { RefreshForegroundTarget(); return 0; }
    if (message == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case kMenuToggle: { auto s = settings_.Snapshot(); s.enabled = !s.enabled; settings_.Apply(s); settings_.Save(); engine_.SetEnabled(s.enabled); RefreshMenuState(); break; }
        case kMenuForeground: ToggleForegroundRule(); break;
        case kMenuSettings: ShowSettings(); break;
        case kMenuStartup: { auto s = settings_.Snapshot(); SetLaunchOnLogin(!s.launchOnLogin); break; }
        case kMenuExit: RequestExit(); break;
        default: break;
        }
        return 0;
    }
    if (message == WM_CLOSE) {
        shuttingDown_ = true; KillTimer(window, kTimerForeground); RemoveTrayIcon();
        if (settingsWindow_) settingsWindow_->Close(); engine_.Stop(); DestroyWindow(window); return 0;
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace flowwheel
