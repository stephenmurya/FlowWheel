# FlowWheel

FlowWheel is an independent Windows tray utility that applies eased, configurable mouse-wheel scrolling across desktop applications. It runs quietly in the notification area and keeps the original wheel input available when smoothing is disabled, an application is excluded, or the input engine cannot safely inject output.

The current **FlowWheel 2.0.1** implementation is a native C++20/Win32 application. Its portable executable is roughly 125 KB and has no bundled runtime. The original self-contained .NET 1.0.0 release remains available from the GitHub releases page.

## What it does

- Smooths vertical and horizontal mouse-wheel input with configurable duration, sensitivity, easing, and optional acceleration.
- Supports Shift + wheel horizontal scrolling and an invert-direction option.
- Provides Natural, Gentle, and Fast presets, plus a **Reset defaults** action.
- Lets you toggle FlowWheel globally from the tray and add a per-application exclusion for the foreground app. Excluded apps receive native pass-through scrolling.
- Detects a conservative set of game executables and pauses smoothing in those games by default. A game can be explicitly allowed through the game-bypass exceptions setting.
- Stores settings in a readable INI file and can launch at Windows sign-in.
- Installs per-user, without requiring administrator approval.

## Requirements

### Running the published build

- Windows 10 or Windows 11, x64.
- No separate runtime installation is required.
- An unsigned build may produce a Windows SmartScreen warning on first launch.

The current build is a desktop input utility. Windows security boundaries such as User Interface Privilege Isolation (UIPI) can prevent it from injecting scroll events into an elevated application. When injection fails, FlowWheel fails open and lets physical scrolling pass through.

### Building from source

- Windows, PowerShell, CMake, and Visual Studio C++ build tools.
- The installer wrapper additionally uses the .NET Framework `csc.exe` included with Visual Studio/Windows developer tooling. The installed application itself is native and does not require .NET.

## Install or run

The GitHub release provides these generated artifacts (local builds use the same paths):

```text
artifacts\native\FlowWheel-Setup.exe   # per-user installer
artifacts\native\FlowWheel.exe         # portable native executable
```

Run the setup executable for the normal installation. The custom bootstrapper installs the application to:

```text
%LOCALAPPDATA%\FlowWheel
```

It creates a Start Menu shortcut, registers a per-user uninstall entry, and offers to launch FlowWheel. The portable executable can be copied and launched directly; it does not create an installation entry.

## Build

From the repository root:

```powershell
.\scripts\build-native.ps1 -Installer
```

The portable executable and installer are written to `artifacts\native`. The older `scripts\build.ps1` command remains available to reproduce the archived .NET 1.0.0 implementation.

## Settings and data

Preferences are saved at:

```text
%LOCALAPPDATA%\FlowWheel\settings.ini
```

FlowWheel does not collect application content, keystrokes, window text, or network telemetry.

The settings window exposes:

- Profile: Natural, Gentle, or Fast.
- Sensitivity, settling duration, and continuous-scroll acceleration.
- Vertical/horizontal smoothing, Shift + wheel horizontal scrolling, and direction inversion.
- Pause in detected games and game-bypass exceptions.
- Process-name exclusions and the current app's tray exclusion command.
- Launch at Windows startup and notification balloons.

## Per-app controls

To pause FlowWheel for the application currently under the pointer, open the FlowWheel tray menu and choose **Disable smooth scrolling in _AppName_**. The same item changes to **Enable smooth scrolling in _AppName_** for an excluded application.

Rules use the full executable path when Windows makes it available, falling back to the process name. This avoids treating two unrelated executables with the same name as the same app where possible.

## Game detection

Game pausing is intentionally conservative. It uses only the foreground process name and executable path, with known game image names, common shipped-game suffixes, and common library folder markers. Steam, Epic, Xbox, and other launchers are excluded from the launcher list so opening a launcher does not pause scrolling. Unknown software is never classified as a game automatically.

This is a heuristic, not a Windows-supported game-presence API. An undetected game can be disabled from the tray menu; a false-positive detection can be explicitly enabled from the same menu.

## Clean-room status and scope

FlowWheel is an independent clean-room implementation. It does not link to, copy, patch, or depend on the expired SmoothScroll executable, DLLs, subscription checks, or proprietary assets. The product name, settings schema, tray UI, and implementation are FlowWheel's own.

It aims to provide comparable user-facing scrolling behavior, not binary or source-level compatibility with SmoothScroll. No SmoothScroll source code is included in this repository. The native implementation preserves this independent implementation boundary.

## Known limitations

- The current release is a self-contained .NET build and is therefore substantially larger than an equivalent native executable.
- It is currently published and tested as a Windows desktop application; macOS and Linux are out of scope.
- Elevated applications may reject `SendInput` from a non-elevated FlowWheel process. FlowWheel reports the fault and restores pass-through behavior rather than swallowing physical wheel input.
- Some applications implement custom scrolling controls and may not respond exactly like a browser or standard Windows view.
- Game classification is conservative and heuristic. It can miss new games and may require a manual exception.
- The published executables are unsigned. SmartScreen and enterprise application-control policies may require an allow-list decision.
- No warranty is made that every third-party application will accept injected wheel events.

## Repository layout

```text
native\                    current C++20 input engine, tray UI, and settings
src\FlowWheel.Core\       archived 1.0.0 .NET input engine
ui\FlowWheel.UI\          archived 1.0.0 .NET tray UI
installer\                 per-user installer wrapper source
scripts\                   native and legacy build scripts
docs\                      architecture and product notes
artifacts\                 generated local publish/install outputs
```

## License and contribution note

This repository is the project source for the independently implemented FlowWheel utility. Do not submit proprietary SmoothScroll code, decompiled code, or license-bypass changes.
