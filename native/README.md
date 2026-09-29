# FlowWheel native application

This directory contains the clean-room native Windows implementation of
FlowWheel 2.0. It uses C++20 and direct Win32 APIs for the mouse hook, eased
output, tray controls, settings window, process rules, and persistence.

The settings file is written to:

```text
%LOCALAPPDATA%\\FlowWheel\\settings.ini
```

`SettingsStore::Load` tolerates missing or malformed values and falls back to
validated defaults. Saves are immediately flushed through the Windows profile
API, and `Reset` restores the neutral profile. The default profile keeps one
standard Windows wheel step, uses a 260 ms cubic settle, leaves acceleration
off, preserves Shift-horizontal scrolling, and bypasses games by default.

## Build

From a Visual Studio Developer PowerShell:

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release
```

The final executable is written to `native/build/Release/FlowWheel.exe`.

This implementation is independent of the commercial SmoothScroll program;
no proprietary binaries, source, or licensing code are used.
