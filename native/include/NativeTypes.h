#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <unordered_set>

namespace flowwheel {

// The tag is deliberately private to FlowWheel.  It prevents our own SendInput events from
// being fed back into the low-level hook.  The injected flags are checked as well because
// other remappers and drivers may not preserve dwExtraInfo.
constexpr ULONG_PTR kInjectionTag = static_cast<ULONG_PTR>(0x464C4F5757484545ULL);

struct ScreenPoint {
    LONG x = 0;
    LONG y = 0;
};

struct MouseWheelEvent {
    SHORT delta = 0;
    bool horizontal = false;
    ScreenPoint screenPoint{};
    HWND targetWindow = nullptr;
    DWORD timestamp = 0;
    bool shiftDown = false;
};

struct FlowWheelOptions {
    bool enabled = true;
    double stepSize = 120.0;
    double animationTime = 360.0;
    bool pulseAlgorithm = true;
    double pulseScale = 3.0;
    double pulseNormalize = 1.0;
    bool accelerationDefault = false;
    double accelerationDelta = 70.0;
    double accelerationMax = 7.0;
    bool reverseDirection = false;
    bool horizontalShiftKey = true;
    bool horizontalSmoothing = true;
    bool globalEnabled = true;
    bool autoDisableGames = true;

    // Names are lower-cased by ScrollEngine before matching.  Keeping this as a value type
    // makes options snapshots cheap and keeps the hook callback independent of the UI thread.
    std::unordered_set<std::wstring> excludedProcessPaths;
    std::unordered_set<std::wstring> excludedProcessNames;
    std::unordered_set<std::wstring> gameBypassExceptions;

    int DurationMilliseconds() const noexcept;
    double SafeStepSize() const noexcept;
    double SafePulseScale() const noexcept;
    double SafePulseNormalize() const noexcept;
    double SafeAccelerationDelta() const noexcept;
    double SafeAccelerationMax() const noexcept;
};

} // namespace flowwheel
