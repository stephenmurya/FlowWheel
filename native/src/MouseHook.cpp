#include "MouseHook.h"

#include <windows.h>

#include <utility>

namespace flowwheel {

std::atomic<MouseHook*> MouseHook::s_active{nullptr};

MouseHook::~MouseHook() {
    Stop();
}

void MouseHook::SetProcessor(Processor processor) {
    std::lock_guard lock(m_mutex);
    m_processor = std::move(processor);
}

bool MouseHook::Start() {
    std::unique_lock lock(m_mutex);
    if (m_started)
        return true;

    // SetWindowsHookEx has process-wide callback state for this class.  Refuse a second
    // instance rather than accidentally routing wheel events to the wrong owner.
    MouseHook* expected = nullptr;
    if (!s_active.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
        return false;

    m_stopping = false;
    m_readyFlag = false;
    m_hook = nullptr;
    m_hookError = ERROR_SUCCESS;
    m_started = true;
    try {
        m_thread = std::thread(&MouseHook::HookThreadMain, this);
    } catch (...) {
        m_started = false;
        s_active.store(nullptr, std::memory_order_release);
        throw;
    }

    if (!m_ready.wait_for(lock, std::chrono::seconds(5), [this] { return m_readyFlag; })) {
        lock.unlock();
        Stop();
        return false;
    }

    const bool ok = m_hook != nullptr && !m_stopping;
    lock.unlock();
    if (!ok)
        Stop();
    return ok;
}

void MouseHook::Stop() noexcept {
    std::thread thread;
    DWORD threadId = 0;
    {
        std::lock_guard lock(m_mutex);
        if (!m_started)
            return;
        m_stopping = true;
        threadId = m_threadId;
        thread = std::move(m_thread);
    }

    if (threadId != 0)
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    if (thread.joinable() && thread.get_id() != std::this_thread::get_id())
        thread.join();
    else if (thread.joinable())
        thread.detach();

    {
        std::lock_guard lock(m_mutex);
        m_started = false;
        m_stopping = false;
        m_threadId = 0;
        m_hook = nullptr;
    }
    MouseHook* expected = this;
    s_active.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

bool MouseHook::IsRunning() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_started && !m_stopping && m_hook != nullptr;
}

LRESULT CALLBACK MouseHook::HookProc(int code, WPARAM wParam, LPARAM lParam) noexcept {
    MouseHook* active = s_active.load(std::memory_order_acquire);
    if (active != nullptr)
        return active->OnHookMessage(code, wParam, lParam);
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT MouseHook::OnHookMessage(int code, WPARAM wParam, LPARAM lParam) noexcept {
    if (code < HC_ACTION || (wParam != WM_MOUSEWHEEL && wParam != WM_MOUSEHWHEEL))
        return CallNextHookEx(nullptr, code, wParam, lParam);

    try {
        const auto* data = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (data == nullptr ||
            (data->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) != 0 ||
            data->dwExtraInfo == kInjectionTag) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        const SHORT delta = static_cast<SHORT>(HIWORD(data->mouseData));
        if (delta == 0)
            return CallNextHookEx(nullptr, code, wParam, lParam);

        POINT point{data->pt.x, data->pt.y};
        HWND target = WindowFromPoint(point);
        if (target != nullptr) {
            HWND root = GetAncestor(target, GA_ROOT);
            if (root != nullptr)
                target = root;
        }

        MouseWheelEvent event;
        event.delta = delta;
        event.horizontal = wParam == WM_MOUSEHWHEEL;
        event.screenPoint = {data->pt.x, data->pt.y};
        event.targetWindow = target;
        event.timestamp = data->time;
        event.shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        Processor processor;
        {
            std::lock_guard lock(m_mutex);
            processor = m_processor;
        }
        // A missing processor or any processor failure is pass-through by design.
        if (processor && processor(event))
            return 1;
    } catch (...) {
        // Low-level hook callbacks must never let a C++ exception cross the Win32 boundary.
        // Returning CallNextHookEx here is the fail-open behavior.
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void MouseHook::HookThreadMain() noexcept {
    const DWORD threadId = GetCurrentThreadId();
    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, &MouseHook::HookProc, GetModuleHandleW(nullptr), 0);
    const DWORD hookError = hook == nullptr ? GetLastError() : ERROR_SUCCESS;

    // Force creation of the queue before signaling Start.  This closes the race where Stop
    // posts WM_QUIT before the queue exists.
    MSG ignored{};
    PeekMessageW(&ignored, nullptr, 0, 0, PM_NOREMOVE);
    {
        std::lock_guard lock(m_mutex);
        m_threadId = threadId;
        m_hook = hook;
        m_hookError = hookError;
        m_readyFlag = true;
    }
    m_ready.notify_all();

    // Stop can legitimately arrive before this thread has published its ID.  Once the
    // message queue exists, close that race by posting the quit message from the hook thread
    // itself instead of leaving Stop waiting forever in join().
    {
        std::lock_guard lock(m_mutex);
        if (m_stopping)
            PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }

    if (hook == nullptr)
        return;

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(hook);
    {
        std::lock_guard lock(m_mutex);
        m_hook = nullptr;
    }
}

} // namespace flowwheel
