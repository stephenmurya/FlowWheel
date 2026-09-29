#include "ScrollEngine.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cwctype>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace flowwheel {
namespace {

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::unordered_set<std::wstring> NormalizeSet(const std::unordered_set<std::wstring>& source) {
    std::unordered_set<std::wstring> result;
    result.reserve(source.size());
    for (const auto& value : source)
        result.insert(Lower(value));
    return result;
}

const std::unordered_set<std::wstring>& KnownGames() {
    static const std::unordered_set<std::wstring> names = {
        L"eldenring.exe", L"valorant-win64-shipping.exe", L"cs2.exe", L"csgo.exe",
        L"fortniteclient-win64-shipping.exe", L"r5apex.exe", L"apex_legends.exe",
        L"overwatch.exe", L"overwatchlauncher.exe", L"robloxplayerbeta.exe", L"rocketleague.exe",
        L"dota2.exe", L"league of legends.exe", L"gta5.exe", L"witcher3.exe", L"cyberpunk2077.exe",
        L"bg3.exe", L"helldivers2.exe", L"starfield.exe", L"destiny2.exe", L"pubg.exe",
        L"rainbowsix.exe", L"thefinals.exe", L"diablo iv.exe", L"fallguys_client_game.exe",
        L"palworld-win64-shipping.exe", L"monsterhunterwilds.exe", L"armoredcore6.exe",
        L"sekiro.exe", L"darksoulsiii.exe", L"hades.exe", L"hades2.exe", L"terraria.exe",
        L"stardew valley.exe", L"minecraft.exe"
    };
    return names;
}

bool IsGameProcess(const std::wstring& name, const std::wstring& path) {
    if (name.empty())
        return false;

    static const std::unordered_set<std::wstring> launchers = {
        L"steam.exe", L"steamwebhelper.exe", L"epicgameslauncher.exe", L"riotclientservices.exe",
        L"battle.net.exe", L"galaxyclient.exe", L"ubisoftconnect.exe", L"ea app.exe", L"xboxapp.exe"
    };
    if (launchers.contains(name))
        return false;
    if (KnownGames().contains(name))
        return true;

    if ((name.size() > 18 && name.ends_with(L"-win64-shipping.exe")) ||
        (name.size() > 14 && name.ends_with(L"-win64-test.exe")) ||
        (name.size() > 9 && name.ends_with(L".game.exe")))
        return true;

    const std::wstring lowerPath = Lower(path);
    return lowerPath.find(L"\\steamapps\\common\\") != std::wstring::npos ||
           lowerPath.find(L"\\epic games\\") != std::wstring::npos ||
           lowerPath.find(L"\\gog galaxy\\games\\") != std::wstring::npos ||
           lowerPath.find(L"\\xboxgames\\") != std::wstring::npos ||
           lowerPath.find(L"\\battle.net\\games\\") != std::wstring::npos ||
           lowerPath.find(L"\\ubisoft game launcher\\games\\") != std::wstring::npos ||
           lowerPath.find(L"\\ea games\\") != std::wstring::npos;
}

} // namespace

bool IsLikelyGameProcess(const std::wstring& executablePath) noexcept {
    try {
        const auto path = Lower(executablePath);
        const auto slash = path.find_last_of(L"\\/");
        const auto name = slash == std::wstring::npos ? path : path.substr(slash + 1);
        return IsGameProcess(name, path);
    } catch (...) {
        return false;
    }
}

int FlowWheelOptions::DurationMilliseconds() const noexcept {
    return static_cast<int>(std::clamp(std::round(animationTime), 20.0, 2000.0));
}

double FlowWheelOptions::SafeStepSize() const noexcept {
    return std::clamp(std::abs(stepSize), 0.0, 4800.0);
}

double FlowWheelOptions::SafePulseScale() const noexcept {
    return std::clamp(pulseScale, 0.0, 12.0);
}

double FlowWheelOptions::SafePulseNormalize() const noexcept {
    return std::clamp(pulseNormalize, 0.0, 12.0);
}

double FlowWheelOptions::SafeAccelerationDelta() const noexcept {
    return std::clamp(std::abs(accelerationDelta), 0.0, 2400.0);
}

double FlowWheelOptions::SafeAccelerationMax() const noexcept {
    return std::clamp(std::abs(accelerationMax), 0.0, 64.0);
}

// The matcher has no hook-thread process inspection.  Refreshes own a shared state object so
// a late refresh cannot touch a destroyed ScrollEngine during shutdown.
class ScrollEngine::ProcessMatcher final {
public:
    struct Entry {
        std::chrono::steady_clock::time_point expires{};
        std::wstring name;
        std::wstring path;
    };

    struct State {
        std::mutex mutex;
        std::unordered_map<HWND, Entry> cache;
        std::unordered_set<HWND> refreshing;
    };

    ProcessMatcher() : m_state(std::make_shared<State>()) {}

    // nullopt means identity is not ready; callers pass the wheel through until refresh ends.
    std::optional<bool> IsExcluded(HWND hwnd, const FlowWheelOptions& options) {
        if (hwnd == nullptr || (!options.autoDisableGames && options.excludedProcessNames.empty() &&
                                options.excludedProcessPaths.empty()))
            return false;

        auto entry = Cached(hwnd);
        if (!entry)
            return std::nullopt;
        if (options.excludedProcessNames.contains(entry->name) ||
            options.excludedProcessPaths.contains(entry->path))
            return true;
        return options.autoDisableGames && !options.gameBypassExceptions.contains(entry->name) &&
               !options.gameBypassExceptions.contains(entry->path) &&
               IsGameProcess(entry->name, entry->path);
    }

    std::optional<bool> IsExplorer(HWND hwnd) {
        if (hwnd == nullptr)
            return false;
        auto entry = Cached(hwnd);
        if (!entry)
            return std::nullopt;
        return entry->name == L"explorer.exe";
    }

private:
    std::optional<Entry> Cached(HWND hwnd) {
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(m_state->mutex);
            auto it = m_state->cache.find(hwnd);
            if (it != m_state->cache.end() && it->second.expires > now)
                return it->second;
            if (m_state->refreshing.insert(hwnd).second) {
                const auto state = m_state;
                std::thread([state, hwnd] { Refresh(state, hwnd); }).detach();
            }
        }
        return std::nullopt;
    }

    static void Refresh(const std::shared_ptr<State>& state, HWND hwnd) noexcept {
        Entry result;
        try {
            DWORD processId = 0;
            GetWindowThreadProcessId(hwnd, &processId);
            if (processId != 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
                if (process != nullptr) {
                    wchar_t path[MAX_PATH * 4]{};
                    DWORD length = static_cast<DWORD>(std::size(path));
                    if (QueryFullProcessImageNameW(process, 0, path, &length) != FALSE) {
                        result.path = Lower(std::wstring(path, length));
                        const auto slash = result.path.find_last_of(L"\\/");
                        result.name = Lower(slash == std::wstring::npos ? result.path :
                                             result.path.substr(slash + 1));
                    }
                    CloseHandle(process);
                }
            }
        } catch (...) {
            // The cache remains an unknown process, which is always fail-open.
        }

        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(state->mutex);
            result.expires = now + std::chrono::seconds(2);
            state->cache[hwnd] = std::move(result);
            state->refreshing.erase(hwnd);
            if (state->cache.size() > 128) {
                for (auto it = state->cache.begin(); it != state->cache.end();) {
                    if (it->second.expires <= now)
                        it = state->cache.erase(it);
                    else
                        ++it;
                }
            }
        }
    }

    std::shared_ptr<State> m_state;
};

ScrollEngine::ScrollEngine(FlowWheelOptions options)
    : m_options(std::move(options)), m_exclusions(std::make_unique<ProcessMatcher>()) {
    m_options.excludedProcessNames = NormalizeSet(m_options.excludedProcessNames);
    m_options.excludedProcessPaths = NormalizeSet(m_options.excludedProcessPaths);
    m_options.gameBypassExceptions = NormalizeSet(m_options.gameBypassExceptions);
    m_hook.SetProcessor([this](const MouseWheelEvent& event) { return HandleWheel(event); });
}

ScrollEngine::~ScrollEngine() {
    Stop();
}

bool ScrollEngine::Start() {
    {
        std::lock_guard lock(m_mutex);
        if (m_running)
            return true;
        m_stopping = false;
        m_animationFaulted = false;
        m_verticalCarry = 0.0;
        m_horizontalCarry = 0.0;
        m_lastWheel = {};
        m_lastDirection = 0;
        m_acceleration = 1.0;
        m_running = true;
    }

    try {
        m_animationThread = std::thread(&ScrollEngine::AnimationThreadMain, this);
        if (!m_hook.Start()) {
            Stop();
            return false;
        }
        return true;
    } catch (...) {
        Stop();
        throw;
    }
}

void ScrollEngine::Stop() noexcept {
    std::thread animation;
    {
        std::lock_guard lock(m_mutex);
        if (!m_running)
            return;
        m_stopping = true;
        m_segments.clear();
        animation = std::move(m_animationThread);
    }

    m_hook.Stop();
    m_wake.notify_all();
    if (animation.joinable() && animation.get_id() != std::this_thread::get_id())
        animation.join();
    else if (animation.joinable())
        animation.detach();

    std::lock_guard lock(m_mutex);
    m_running = false;
    m_stopping = false;
    m_animationFaulted = false;
    m_verticalCarry = 0.0;
    m_horizontalCarry = 0.0;
}

void ScrollEngine::UpdateOptions(FlowWheelOptions options) {
    std::lock_guard lock(m_mutex);
    options.excludedProcessNames = NormalizeSet(options.excludedProcessNames);
    options.excludedProcessPaths = NormalizeSet(options.excludedProcessPaths);
    options.gameBypassExceptions = NormalizeSet(options.gameBypassExceptions);
    m_options = std::move(options);
    m_wake.notify_all();
}

FlowWheelOptions ScrollEngine::Options() const {
    std::lock_guard lock(m_mutex);
    return m_options;
}

bool ScrollEngine::IsRunning() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_running && !m_stopping;
}

bool ScrollEngine::HandleWheel(const MouseWheelEvent& input) noexcept {
    try {
        FlowWheelOptions options;
        {
            std::lock_guard lock(m_mutex);
            options = m_options;
            if (!m_running || m_stopping || m_animationFaulted || !options.enabled || !options.globalEnabled)
                return false;
        }

        const auto excluded = m_exclusions->IsExcluded(input.targetWindow, options);
        if (!excluded.has_value() || *excluded)
            return false;

        const bool explorer = m_exclusions->IsExplorer(input.targetWindow).value_or(false);
        bool horizontal = input.horizontal;
        if (!horizontal && input.shiftDown && options.horizontalShiftKey)
            horizontal = true;
        if (horizontal && !options.horizontalSmoothing)
            return false;

        int direction = input.delta < 0 ? -1 : 1;
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(m_mutex);
        if (options.reverseDirection)
            direction = -direction;
        const auto elapsed = m_lastWheel.time_since_epoch().count() == 0
                                 ? 1e100
                                 : std::chrono::duration<double, std::milli>(now - m_lastWheel).count();
        if (!options.accelerationDefault || direction != m_lastDirection || elapsed > 220.0)
            m_acceleration = 1.0;
        else if (options.SafeAccelerationDelta() > 0.0)
            m_acceleration = std::min(options.SafeAccelerationMax(),
                                      m_acceleration + std::abs(static_cast<double>(input.delta)) /
                                                            options.SafeAccelerationDelta());
        m_lastWheel = now;
        m_lastDirection = direction;

        const double magnitude = std::abs(static_cast<double>(input.delta)) * options.SafeStepSize() / 120.0 *
                                 m_acceleration;
        if (magnitude <= 0.0)
            return true;
        if (m_stopping || !m_running)
            return false;
        if (m_segments.size() >= 256)
            m_segments.erase(m_segments.begin(), m_segments.end() - 255);
        m_segments.push_back({direction * magnitude, horizontal, explorer, now,
                              std::chrono::milliseconds(options.DurationMilliseconds()), 0.0});
        m_wake.notify_one();
        return true;
    } catch (...) {
        // Never swallow physical input if the processor cannot enqueue it.
        return false;
    }
}

void ScrollEngine::AnimationThreadMain() noexcept {
    try {
        std::unique_lock lock(m_mutex);
        while (!m_stopping) {
            lock.unlock();
            Tick();
            lock.lock();
            m_wake.wait_for(lock, std::chrono::milliseconds(8));
        }
    } catch (...) {
        std::lock_guard lock(m_mutex);
        m_animationFaulted = true;
        m_segments.clear();
        m_verticalCarry = 0.0;
        m_horizontalCarry = 0.0;
    }
}

void ScrollEngine::Tick() {
    const auto now = std::chrono::steady_clock::now();
    double vertical = 0.0;
    double horizontal = 0.0;
    double explorerVertical = 0.0;
    double explorerHorizontal = 0.0;
    FlowWheelOptions options;
    {
        std::lock_guard lock(m_mutex);
        options = m_options;
        for (std::size_t index = m_segments.size(); index-- > 0;) {
            auto& segment = m_segments[index];
            const double progress = std::clamp(
                std::chrono::duration<double, std::milli>(now - segment.start).count() /
                    static_cast<double>(segment.duration.count()),
                0.0, 1.0);
            if (segment.explorerCompatibility) {
                if (progress >= 1.0) {
                    if (segment.horizontal) explorerHorizontal += segment.delta;
                    else explorerVertical += segment.delta;
                    m_segments.erase(m_segments.begin() + static_cast<std::ptrdiff_t>(index));
                }
                continue;
            }
            const double emitted = segment.delta * Ease(progress, options);
            const double increment = emitted - segment.emitted;
            segment.emitted = emitted;
            if (segment.horizontal) horizontal += increment;
            else vertical += increment;
            if (progress >= 1.0)
                m_segments.erase(m_segments.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    m_verticalCarry += vertical;
    m_horizontalCarry += horizontal;
    const int verticalUnits = TakeWhole(m_verticalCarry);
    const int horizontalUnits = TakeWhole(m_horizontalCarry);
    if (verticalUnits != 0) InjectWheel(verticalUnits, false);
    if (horizontalUnits != 0) InjectWheel(horizontalUnits, true);

    const int explorerVerticalUnits = ToLegacyWheelDelta(explorerVertical);
    const int explorerHorizontalUnits = ToLegacyWheelDelta(explorerHorizontal);
    if (explorerVerticalUnits != 0) InjectWheel(explorerVerticalUnits, false);
    if (explorerHorizontalUnits != 0) InjectWheel(explorerHorizontalUnits, true);
}

double ScrollEngine::Ease(double progress, const FlowWheelOptions& options) noexcept {
    if (!options.pulseAlgorithm)
        return progress;
    const double scale = std::max(0.05, options.SafePulseScale());
    const double exponent = 1.0 + 2.0 / scale;
    const double pulse = 1.0 - std::pow(1.0 - progress, exponent);
    const double normalize = std::max(0.0001, options.SafePulseNormalize());
    return std::clamp(pulse * normalize + progress * (1.0 - normalize), 0.0, 1.0);
}

int ScrollEngine::TakeWhole(double& carry) noexcept {
    const double whole = std::trunc(carry);
    carry -= whole;
    return static_cast<int>(std::clamp(whole, static_cast<double>(INT_MIN), static_cast<double>(INT_MAX)));
}

int ScrollEngine::ToLegacyWheelDelta(double delta) noexcept {
    if (std::abs(delta) < 0.5)
        return 0;
    const auto clicks = std::max(1L, static_cast<long>(std::llround(std::abs(delta) / 120.0)));
    const auto bounded = std::min(clicks, static_cast<long>(INT_MAX / 120));
    return static_cast<int>((delta < 0.0 ? -1L : 1L) * bounded * 120L);
}

void ScrollEngine::InjectWheel(int delta, bool horizontal) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = static_cast<DWORD>(delta);
    input.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
    input.mi.dwExtraInfo = kInjectionTag;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        // This includes UIPI/elevated-target failure.  Disable interception until restart so
        // a physical wheel can never remain swallowed after injection stops working.
        std::lock_guard lock(m_mutex);
        m_animationFaulted = true;
        m_segments.clear();
        m_verticalCarry = 0.0;
        m_horizontalCarry = 0.0;
    }
}

} // namespace flowwheel
