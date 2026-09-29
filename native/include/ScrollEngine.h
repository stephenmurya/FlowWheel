#pragma once

#include "MouseHook.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace flowwheel {

// Uses the same conservative filename/path matcher as the runtime policy. The
// tray uses this to explain an automatic game bypass and to persist an opt-out
// exception when the user enables smoothing for that game.
bool IsLikelyGameProcess(const std::wstring& executablePath) noexcept;

class ScrollEngine final {
public:
    explicit ScrollEngine(FlowWheelOptions options = {});
    ~ScrollEngine();

    ScrollEngine(const ScrollEngine&) = delete;
    ScrollEngine& operator=(const ScrollEngine&) = delete;

    bool Start();
    void Stop() noexcept;
    void UpdateOptions(FlowWheelOptions options);
    FlowWheelOptions Options() const;
    bool IsRunning() const noexcept;

private:
    struct Segment {
        double delta = 0.0;
        bool horizontal = false;
        bool explorerCompatibility = false;
        std::chrono::steady_clock::time_point start{};
        std::chrono::milliseconds duration{1};
        double emitted = 0.0;
    };

    class ProcessMatcher;

    bool HandleWheel(const MouseWheelEvent& input) noexcept;
    void AnimationThreadMain() noexcept;
    void Tick();
    void InjectWheel(int delta, bool horizontal);

    static double Ease(double progress, const FlowWheelOptions& options) noexcept;
    static int TakeWhole(double& carry) noexcept;
    static int ToLegacyWheelDelta(double delta) noexcept;

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    MouseHook m_hook;
    std::unique_ptr<ProcessMatcher> m_exclusions;
    std::thread m_animationThread;
    FlowWheelOptions m_options;
    std::vector<Segment> m_segments;
    bool m_running = false;
    bool m_stopping = false;
    bool m_animationFaulted = false;
    double m_verticalCarry = 0.0;
    double m_horizontalCarry = 0.0;
    int m_lastDirection = 0;
    std::chrono::steady_clock::time_point m_lastWheel{};
    double m_acceleration = 1.0;
};

} // namespace flowwheel
