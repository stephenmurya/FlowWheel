using System.Diagnostics;
using System.Runtime.InteropServices;

namespace FlowWheel;

/// <summary>Small, defensive snapshot of the executable under the pointer or in the foreground.</summary>
internal sealed record AppTarget(nint Window, string ProcessName, string ExecutablePath, string DisplayName)
{
    public bool HasExecutablePath => !string.IsNullOrWhiteSpace(ExecutablePath);
}

internal static class AppTargetResolver
{
    private const uint GaRoot = 2;

    public static AppTarget? GetForegroundTarget(int ownProcessId)
    {
        return GetTarget(GetForegroundWindow(), ownProcessId);
    }

    public static AppTarget? GetTargetAt(Point point, int ownProcessId)
    {
        return GetTarget(WindowFromPoint(new NativePoint { X = point.X, Y = point.Y }), ownProcessId);
    }

    private static AppTarget? GetTarget(nint window, int ownProcessId)
    {
        if (window == 0) return null;
        var root = GetAncestor(window, GaRoot);
        if (root != 0) window = root;

        GetWindowThreadProcessId(window, out var processId);
        if (processId == 0 || processId == (uint)ownProcessId) return null;
        var className = GetClassName(window);
        // A tray click briefly makes Explorer's shell chrome the foreground window. Keep the
        // last real app in that case so the command applies to the app the user was using.
        if (IsShellChrome(className)) return null;

        try
        {
            using var process = Process.GetProcessById((int)processId);
            var processName = process.ProcessName + ".exe";
            var path = string.Empty;
            try { path = process.MainModule?.FileName ?? string.Empty; } catch { /* UIPI / exited process */ }
            return new AppTarget(window, processName, path, GetDisplayName(processName, path, className));
        }
        catch
        {
            // A process can exit between GetWindowThreadProcessId and Process.GetProcessById.
            return null;
        }
    }

    private static string GetDisplayName(string processName, string path, string className)
    {
        if (processName.Equals("explorer.exe", StringComparison.OrdinalIgnoreCase) &&
            className.Equals("CabinetWClass", StringComparison.OrdinalIgnoreCase))
            return "File Explorer";

        if (!string.IsNullOrWhiteSpace(path))
        {
            try
            {
                var description = FileVersionInfo.GetVersionInfo(path).FileDescription;
                if (!string.IsNullOrWhiteSpace(description)) return description.Trim();
            }
            catch { /* version metadata is optional */ }
        }

        return Path.GetFileNameWithoutExtension(processName);
    }

    private static bool IsShellChrome(string className) =>
        className.Equals("Shell_TrayWnd", StringComparison.OrdinalIgnoreCase) ||
        className.Equals("NotifyIconOverflowWindow", StringComparison.OrdinalIgnoreCase) ||
        className.Equals("Progman", StringComparison.OrdinalIgnoreCase) ||
        className.Equals("WorkerW", StringComparison.OrdinalIgnoreCase) ||
        className.Equals("DV2ControlHost", StringComparison.OrdinalIgnoreCase) ||
        className.Equals("Windows.UI.Core.CoreWindow", StringComparison.OrdinalIgnoreCase);

    private static string GetClassName(nint window)
    {
        var buffer = new char[256];
        var length = GetClassNameW(window, buffer, buffer.Length);
        return length > 0 ? new string(buffer, 0, length) : string.Empty;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativePoint
    {
        public int X;
        public int Y;
    }

    [DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern nint WindowFromPoint(NativePoint point);

    [DllImport("user32.dll")]
    private static extern nint GetAncestor(nint window, uint flags);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint GetWindowThreadProcessId(nint window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassNameW(nint window, [Out] char[] className, int maxCount);
}
