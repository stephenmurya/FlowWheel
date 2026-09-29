using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using FlowWheel.Core.Interop;

namespace FlowWheel.Core;

/// <summary>
/// Owns a WH_MOUSE_LL hook on a dedicated message-pump thread. The callback never waits on
/// the animation loop; a processor returning true means that the source wheel message is eaten.
/// </summary>
public sealed class GlobalMouseHook : IDisposable
{
    internal const ulong InjectionTagValue = 0x464C4F5757484545UL; // "FLOWWHEE"
    internal static readonly UIntPtr InjectionTag = new(InjectionTagValue);

    private readonly object _gate = new();
    private readonly ManualResetEventSlim _ready = new(false);
    private NativeMethods.LowLevelMouseProc? _callback;
    private Thread? _thread;
    private uint _threadId;
    private nint _hook;
    private int _hookError;
    private bool _started;
    private bool _stopping;
    private Func<MouseWheelEvent, bool>? _processor;

    public event Action<Exception>? Faulted;

    public bool IsRunning
    {
        get { lock (_gate) return _started && !_stopping; }
    }

    public void SetProcessor(Func<MouseWheelEvent, bool>? processor)
    {
        lock (_gate) _processor = processor;
    }

    public void Start()
    {
        lock (_gate)
        {
            if (_started) return;
            _stopping = false;
            _ready.Reset();
            _callback = HookCallback;
            _thread = new Thread(HookThreadMain)
            {
                IsBackground = true,
                Name = "FlowWheel global mouse hook"
            };
            _started = true;
            _thread.Start();
        }

        if (!_ready.Wait(TimeSpan.FromSeconds(5)))
        {
            Stop();
            throw new Win32Exception("Timed out while starting the FlowWheel mouse hook.");
        }

        int hookError;
        bool hookFailed;
        lock (_gate)
        {
            hookFailed = _hook == 0;
            hookError = _hookError;
        }
        if (hookFailed || !IsRunning)
        {
            Stop();
            throw new Win32Exception(hookError, "Unable to install the FlowWheel mouse hook.");
        }
    }

    public void Stop()
    {
        Thread? thread;
        uint threadId;
        lock (_gate)
        {
            if (!_started) return;
            _stopping = true;
            thread = _thread;
            threadId = _threadId;
        }

        // PostThreadMessage can race with initialization, so the hook thread also exits when
        // _stopping is observed after GetMessage returns. Start waits until initialization ends.
        if (threadId != 0)
            _ = NativeMethods.PostThreadMessage(threadId, NativeMethods.WM_QUIT, 0, 0);
        if (thread is not null && thread != Thread.CurrentThread)
            thread.Join(TimeSpan.FromSeconds(5));

        lock (_gate)
        {
            _started = false;
            _thread = null;
            _threadId = 0;
            _callback = null;
        }
    }

    public void Dispose()
    {
        Stop();
        _ready.Dispose();
    }

    private void HookThreadMain()
    {
        var threadId = NativeMethods.GetCurrentThreadId();
        lock (_gate) _threadId = threadId;

        nint hook = NativeMethods.SetWindowsHookExW(
            NativeMethods.WH_MOUSE_LL,
            _callback!,
            NativeMethods.GetModuleHandle(null),
            0);
        // Capture immediately: PeekMessage/GetMessage and lock operations can overwrite the
        // thread's last-error value before Start() inspects it.
        var hookError = hook == 0 ? Marshal.GetLastWin32Error() : 0;
        lock (_gate)
        {
            _hook = hook;
            _hookError = hookError;
        }
        // Creating the queue before publishing readiness removes the PostThreadMessage race
        // that otherwise makes a very fast Stop occasionally leave the hook thread blocked.
        NativeMethods.PeekMessage(out _, 0, 0, 0, NativeMethods.PM_NOREMOVE);
        _ready.Set();

        if (hook == 0)
            return;

        try
        {
            int result;
            while ((result = NativeMethods.GetMessage(out var msg, 0, 0, 0)) > 0)
            {
                NativeMethods.TranslateMessage(ref msg);
                NativeMethods.DispatchMessage(ref msg);
            }
            if (result < 0)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "FlowWheel mouse hook message pump failed.");
        }
        catch (Exception exception)
        {
            try { Faulted?.Invoke(exception); } catch { /* hook failures must not escape the thread */ }
        }
        finally
        {
            NativeMethods.UnhookWindowsHookEx(hook);
            lock (_gate) _hook = 0;
        }
    }

    private nint HookCallback(int nCode, nint wParam, nint lParam)
    {
        if (nCode < NativeMethods.HC_ACTION ||
            (uint)wParam is not NativeMethods.WM_MOUSEWHEEL and not NativeMethods.WM_MOUSEHWHEEL)
            return NativeMethods.CallNextHookEx(0, nCode, wParam, lParam);

        try
        {
            var data = Marshal.PtrToStructure<NativeMethods.MsllHookStruct>(lParam);
            // Never consume synthetic wheel input.  SendInput marks generated input with
            // LLMHF_INJECTED (and lower-integrity senders may set bit 1); relying only on
            // dwExtraInfo is fragile because some drivers/remappers do not preserve it. If
            // synthetic input is fed back into the smoother it can recurse until the queue is
            // full, and a failed injection can otherwise make the physical wheel appear dead.
            if ((data.Flags & (NativeMethods.LLMHF_INJECTED | NativeMethods.LLMHF_LOWER_IL_INJECTED)) != 0 ||
                data.DwExtraInfo == InjectionTag)
                return NativeMethods.CallNextHookEx(0, nCode, wParam, lParam);

            var horizontal = (uint)wParam == NativeMethods.WM_MOUSEHWHEEL;
            var delta = unchecked((short)(data.MouseData >> 16));
            if (delta == 0)
                return NativeMethods.CallNextHookEx(0, nCode, wParam, lParam);

            var point = new ScreenPoint(data.Pt.X, data.Pt.Y);
            var target = NativeMethods.WindowFromPoint(new NativeMethods.Point { X = data.Pt.X, Y = data.Pt.Y });
            // File Explorer and XAML/UWP apps often deliver the wheel over a child control.
            // Match the top-level owner so process identity remains stable across view changes.
            var rootTarget = NativeMethods.GetAncestor(target, NativeMethods.GA_ROOT);
            if (rootTarget != 0) target = rootTarget;
            var wheelEvent = new MouseWheelEvent(
                delta,
                horizontal,
                point,
                target,
                data.Time,
                (NativeMethods.GetKeyState((int)NativeMethods.WM_KEYSHIFT) & 0x8000) != 0);

            var processor = _processor;
            if (processor is not null && processor(wheelEvent))
                return 1;
        }
        catch (Exception exception)
        {
            try { Faulted?.Invoke(exception); } catch { /* hook callbacks must not escape */ }
        }

        return NativeMethods.CallNextHookEx(0, nCode, wParam, lParam);
    }
}

/// <summary>Asynchronous, cached process matcher that never inspects a process on the hook thread.</summary>
internal sealed class ProcessExclusionMatcher
{
    private readonly object _gate = new();
    private readonly Dictionary<nint, (DateTime Expires, string Name, string Path)> _cache = new();
    private readonly HashSet<nint> _refreshing = new();

    public bool? TryIsExcluded(nint hwnd, IReadOnlySet<string> names, IReadOnlySet<string> paths)
        => TryIsExcluded(hwnd, names, paths, false, new HashSet<string>(StringComparer.OrdinalIgnoreCase));

    public bool? TryIsExcluded(
        nint hwnd,
        IReadOnlySet<string> names,
        IReadOnlySet<string> paths,
        bool autoDisableGames,
        IReadOnlySet<string> gameBypassExceptions)
    {
        if (hwnd == 0 || (!autoDisableGames && names.Count == 0 && paths.Count == 0)) return false;
        var now = DateTime.UtcNow;

        lock (_gate)
        {
            if (_cache.TryGetValue(hwnd, out var cached) && cached.Expires > now)
            {
                if (names.Contains(cached.Name) || paths.Contains(cached.Path)) return true;
                return autoDisableGames &&
                       !gameBypassExceptions.Contains(cached.Name) &&
                       !gameBypassExceptions.Contains(cached.Path) &&
                       GameBypassClassifier.IsGameProcess(cached.Name, cached.Path);
            }

            if (_refreshing.Add(hwnd))
                _ = Task.Run(() => Refresh(hwnd));
        }

        // Unknown means pass-through for this event. The next event uses the completed cache,
        // preserving the hook's low-latency contract even when process inspection is slow.
        return null;
    }

    /// <summary>
    /// Identifies the Windows shell process without doing process inspection on the hook
    /// callback. Explorer's file-view controls are legacy wheel consumers: unlike browsers,
    /// many of them require a complete WHEEL_DELTA before they repaint.
    /// </summary>
    public bool? TryIsExplorer(nint hwnd)
    {
        if (hwnd == 0) return false;
        var now = DateTime.UtcNow;

        lock (_gate)
        {
            if (_cache.TryGetValue(hwnd, out var cached) && cached.Expires > now)
                return string.Equals(cached.Name, "explorer.exe", StringComparison.OrdinalIgnoreCase);

            if (_refreshing.Add(hwnd))
                _ = Task.Run(() => Refresh(hwnd));
        }

        // Unknown is intentionally pass-through for the app-specific compatibility mode. The
        // process identity is available for subsequent wheel events without delaying this hook.
        return null;
    }

    private void Refresh(nint hwnd)
    {
        try
        {
            var now = DateTime.UtcNow;
            var processName = string.Empty;
            var processPath = string.Empty;
            NativeMethods.GetWindowThreadProcessId(hwnd, out var pid);
            if (pid != 0)
            {
                try
                {
                    using var process = Process.GetProcessById((int)pid);
                    processName = process.ProcessName + ".exe";
                    try { processPath = process.MainModule?.FileName ?? string.Empty; } catch { /* access denied */ }
                }
                catch { /* process may exit between the window and process lookups */ }
            }

            lock (_gate)
            {
                _cache[hwnd] = (now.AddSeconds(2), processName, processPath);
                if (_cache.Count > 128)
                    foreach (var stale in _cache.Where(pair => pair.Value.Expires <= now).Select(pair => pair.Key).ToArray())
                        _cache.Remove(stale);
            }
        }
        finally
        {
            lock (_gate) _refreshing.Remove(hwnd);
        }
    }
}

/// <summary>
/// Conservative, local game detection. It deliberately uses only the process image name and
/// path already collected for app exclusions. Unknown applications are never classified as games.
/// </summary>
public static class GameBypassClassifier
{
    private static readonly HashSet<string> KnownGameProcesses = new(StringComparer.OrdinalIgnoreCase)
    {
        "eldenring.exe", "valorant-win64-shipping.exe", "cs2.exe", "csgo.exe",
        "fortniteclient-win64-shipping.exe", "r5apex.exe", "apex_legends.exe",
        "overwatch.exe", "overwatchlauncher.exe", "robloxplayerbeta.exe", "rocketleague.exe",
        "dota2.exe", "league of legends.exe", "gta5.exe", "witcher3.exe", "cyberpunk2077.exe", "bg3.exe",
        "helldivers2.exe", "starfield.exe", "destiny2.exe", "pubg.exe", "rainbowsix.exe",
        "thefinals.exe", "diablo iv.exe", "fallguys_client_game.exe", "palworld-win64-shipping.exe",
        "monsterhunterwilds.exe", "armoredcore6.exe", "sekiro.exe", "darksoulsiii.exe",
        "hades.exe", "hades2.exe", "terraria.exe", "stardew valley.exe", "minecraft.exe"
    };

    private static readonly string[] GamePathMarkers =
    {
        "\\steamapps\\common\\", "\\epic games\\", "\\gog galaxy\\games\\",
        "\\xboxgames\\", "\\battle.net\\games\\",
        "\\ubisoft game launcher\\games\\", "\\ea games\\"
    };

    private static readonly HashSet<string> KnownLaunchers = new(StringComparer.OrdinalIgnoreCase)
    {
        "steam.exe", "steamwebhelper.exe", "epicgameslauncher.exe", "riotclientservices.exe",
        "battle.net.exe", "galaxyclient.exe", "ubisoftconnect.exe", "ea app.exe", "xboxapp.exe"
    };

    public static bool IsGameProcess(string processName, string processPath)
    {
        if (string.IsNullOrWhiteSpace(processName)) return false;
        if (KnownLaunchers.Contains(processName)) return false;
        if (KnownGameProcesses.Contains(processName)) return true;

        // Unreal/Unity shipped game binaries commonly carry these suffixes. Keep this narrow;
        // broad substring checks such as "game" incorrectly catch editors and launchers.
        var lowerName = processName.ToLowerInvariant();
        if (lowerName.EndsWith("-win64-shipping.exe", StringComparison.Ordinal) ||
            lowerName.EndsWith("-win64-test.exe", StringComparison.Ordinal) ||
            lowerName.EndsWith(".game.exe", StringComparison.Ordinal))
            return true;

        if (string.IsNullOrWhiteSpace(processPath)) return false;
        var lowerPath = processPath.ToLowerInvariant();
        return GamePathMarkers.Any(marker => lowerPath.Contains(marker, StringComparison.Ordinal));
    }
}
