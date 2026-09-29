using System.Runtime.InteropServices;

namespace FlowWheel.Core.Interop;

internal static class NativeMethods
{
    internal const int WH_MOUSE_LL = 14;
    internal const int HC_ACTION = 0;
    internal const uint WM_QUIT = 0x0012;
    internal const uint WM_MOUSEWHEEL = 0x020A;
    internal const uint WM_MOUSEHWHEEL = 0x020E;
    internal const uint WM_KEYSHIFT = 0x10;
    internal const uint PM_NOREMOVE = 0x0000;
    internal const uint GA_ROOT = 2;
    internal const uint LLMHF_INJECTED = 0x00000001;
    // Set when the input was injected from a lower-integrity process.  Both bits
    // identify synthetic input and must be allowed through the hook unchanged.
    internal const uint LLMHF_LOWER_IL_INJECTED = 0x00000002;
    internal const uint MOUSEEVENTF_WHEEL = 0x0800;
    internal const uint MOUSEEVENTF_HWHEEL = 0x01000;
    internal const uint INPUT_MOUSE = 0;

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    internal delegate nint LowLevelMouseProc(int nCode, nint wParam, nint lParam);

    [StructLayout(LayoutKind.Sequential)]
    internal struct Point
    {
        internal int X;
        internal int Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MsllHookStruct
    {
        internal Point Pt;
        internal uint MouseData;
        internal uint Flags;
        internal uint Time;
        internal UIntPtr DwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Msg
    {
        internal nint Hwnd;
        internal uint Message;
        internal nuint WParam;
        internal nint LParam;
        internal uint Time;
        internal Point Pt;
        internal uint Private;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MouseInput
    {
        internal int Dx;
        internal int Dy;
        internal uint MouseData;
        internal uint DwFlags;
        internal uint Time;
        internal UIntPtr DwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Input
    {
        internal uint Type;
        internal MouseInput Mi;
    }

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern nint SetWindowsHookExW(int idHook, LowLevelMouseProc lpfn, nint hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnhookWindowsHookEx(nint hhk);

    [DllImport("user32.dll")]
    internal static extern nint CallNextHookEx(nint hhk, int nCode, nint wParam, nint lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    internal static extern nint GetModuleHandle(string? lpModuleName);

    [DllImport("kernel32.dll")]
    internal static extern uint GetCurrentThreadId();

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool PostThreadMessage(uint idThread, uint msg, nuint wParam, nint lParam);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern int GetMessage(out Msg lpMsg, nint hWnd, uint wMsgFilterMin, uint wMsgFilterMax);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool PeekMessage(out Msg lpMsg, nint hWnd, uint wMsgFilterMin, uint wMsgFilterMax, uint wRemoveMsg);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool TranslateMessage(ref Msg lpMsg);

    [DllImport("user32.dll")]
    internal static extern nint DispatchMessage(ref Msg lpMsg);

    [DllImport("user32.dll")]
    internal static extern nint WindowFromPoint(Point point);

    [DllImport("user32.dll")]
    internal static extern nint GetAncestor(nint hWnd, uint gaFlags);

    [DllImport("user32.dll")]
    internal static extern uint GetWindowThreadProcessId(nint hWnd, out uint processId);

    [DllImport("user32.dll")]
    internal static extern short GetKeyState(int nVirtKey);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern uint SendInput(uint cInputs, [In] Input[] pInputs, int cbSize);
}
