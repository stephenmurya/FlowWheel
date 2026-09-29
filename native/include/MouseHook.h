#pragma once

#include "NativeTypes.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace flowwheel {

// Owns the WH_MOUSE_LL hook and its message-pump thread.  The hook callback must remain
// extremely small: it validates the message, copies its data, and invokes the non-blocking
// processor.  Returning false always calls CallNextHookEx, which is the fail-open path.
class MouseHook final {
public:
    using Processor = std::function<bool(const MouseWheelEvent&)>;

    MouseHook() = default;
    ~MouseHook();

    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    void SetProcessor(Processor processor);
    bool Start();
    void Stop() noexcept;
    bool IsRunning() const noexcept;

private:
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam) noexcept;
    LRESULT OnHookMessage(int code, WPARAM wParam, LPARAM lParam) noexcept;
    void HookThreadMain() noexcept;

    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    Processor m_processor;
    std::thread m_thread;
    HHOOK m_hook = nullptr;
    DWORD m_threadId = 0;
    DWORD m_hookError = ERROR_SUCCESS;
    bool m_started = false;
    bool m_stopping = false;
    bool m_readyFlag = false;

    // WH_MOUSE_LL callbacks are delivered to the thread that installed the hook.  There is
    // one process-wide callback entry point, so this pointer is published only while running.
    static std::atomic<MouseHook*> s_active;
};

} // namespace flowwheel
